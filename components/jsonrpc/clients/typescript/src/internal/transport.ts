// === transport.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { ConnectionState } from "../client.js";
import type { ReconnectOptions, ReportError } from "../connect.js";
import { JsonRpcError, TimeoutError, TransportError } from "../errors.js";
import { makeIdempotent } from "./cancel.js";
import { defaultReportError, normalizeError } from "./report_error.js";
import type { CancelFn } from "../values.js";

// Minimal WebSocket shape: avoids DOM-type dependence; native browser/Node 22+ WebSocket and the
// test MockWebSocket both satisfy it.
export interface WebSocketLike {
  readonly readyState: number;
  send(data: string): void;
  close(code?: number, reason?: string): void;
  addEventListener(type: "open", listener: () => void): void;
  addEventListener(type: "message", listener: (event: { data: unknown }) => void): void;
  addEventListener(type: "close", listener: (event: { code: number; reason: string }) => void): void;
  addEventListener(type: "error", listener: (event: unknown) => void): void;
}

export type WebSocketFactory = (url: string) => WebSocketLike;

const defaultWebSocketFactory: WebSocketFactory = (url) => {
  const ctor = (globalThis as { WebSocket?: new (url: string) => WebSocketLike }).WebSocket;
  if (!ctor) {
    throw new TransportError(
      "no global WebSocket implementation available (need a browser or Node >=22)",
    );
  }
  return new ctor(url);
};

// JSON-RPC 2.0 envelope shapes.
interface JsonRpcRequest {
  jsonrpc: "2.0";
  id: number;
  method: string;
  params?: unknown;
}

interface JsonRpcSuccessResponse {
  jsonrpc: "2.0";
  id: number;
  result: unknown;
}

interface JsonRpcErrorResponse {
  jsonrpc: "2.0";
  id: number;
  error: { code: number; message: string; data?: unknown };
}

interface JsonRpcNotification {
  jsonrpc: "2.0";
  method: string;
  params?: unknown;
}

// 2s = ~120 kernel cycles at 60Hz: well past any legitimate internal protocol op, quick to flag a hang.
// User-initiated `invoke` calls bypass this default via `timeoutMs: 0` (see ObjectHandle.invoke); the
// timeoutMs > 0 guard below skips the timer for the no-timeout path.
const defaultTimeoutMs = 2_000;
const defaultInitialReconnectDelayMs = 100;
const defaultMaxReconnectDelayMs = 5_000;
const defaultReconnectFactor = 2;

interface TransportOptions {
  url: string;
  defaultTimeoutMs?: number;
  reconnect?: ReconnectOptions;
  webSocketFactory?: WebSocketFactory;
  reportError?: ReportError;
  /** When `true`, also retry the *initial* open using the reconnect backoff schedule
   *  (default: initial open is one-shot, only post-open drops are retried). The whenOpen
   *  promise stays pending across retries; the surrounding `connect()` budget enforces
   *  the total deadline via `openTimeoutMs`. */
  retryInitialOpen?: boolean;
}

type NotificationHandler = (params: unknown) => void;


interface PendingCall {
  resolve: (value: unknown) => void;
  reject: (err: Error) => void;
  timeoutHandle: ReturnType<typeof setTimeout> | null;
  abortListener: (() => void) | null;
  signal: AbortSignal | null;
}

/**
 * WebSocket + JSON-RPC envelope + request/response correlation + autonomous reconnect.
 * Application-level state across reconnects is the layer above; signal it via on{Disconnect,Reconnect}.
 */
export class Transport {
  private readonly url: string;
  private readonly callTimeoutMs: number;
  private readonly reconnect: Required<ReconnectOptions>;
  private readonly retryInitialOpen: boolean;
  private readonly factory: WebSocketFactory;
  private readonly reportError: ReportError;

  private socket: WebSocketLike | null = null;
  private state: ConnectionState = "connecting";
  private nextId = 1;
  private hasEverConnected = false;
  private reconnectAttempt = 0;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  // Captured on 'error', consumed as `cause` on the TransportError thrown for the following 'close'.
  private lastWebSocketError: unknown = null;

  private readonly pending = new Map<number, PendingCall>();
  private readonly notificationHandlers = new Map<string, Set<NotificationHandler>>();
  private readonly disconnectHandlers = new Set<() => void>();
  private readonly reconnectHandlers = new Set<() => void>();
  private readonly connectionStateHandlers = new Set<(state: ConnectionState) => void>();

  private readonly openPromise: Promise<void>;
  private resolveOpen!: () => void;
  private rejectOpen!: (err: Error) => void;

  constructor(options: TransportOptions) {
    this.url = options.url;
    this.callTimeoutMs = options.defaultTimeoutMs ?? defaultTimeoutMs;
    this.reconnect = {
      enabled: options.reconnect?.enabled ?? true,
      initialDelayMs: options.reconnect?.initialDelayMs ?? defaultInitialReconnectDelayMs,
      maxDelayMs: options.reconnect?.maxDelayMs ?? defaultMaxReconnectDelayMs,
      factor: options.reconnect?.factor ?? defaultReconnectFactor,
      autoReestablish: options.reconnect?.autoReestablish ?? true,
    };
    this.retryInitialOpen = options.retryInitialOpen ?? false;
    this.factory = options.webSocketFactory ?? defaultWebSocketFactory;
    this.reportError = options.reportError ?? defaultReportError;
    this.openPromise = new Promise<void>((resolve, reject) => {
      this.resolveOpen = resolve;
      this.rejectOpen = reject;
    });
    // Absorb unhandled-rejection; real awaiters still see it via replay.
    this.openPromise.catch(() => undefined);
    this.openSocket();
  }

  /** First-open promise; does not re-fulfill on later reconnects. */
  whenOpen(): Promise<void> {
    return this.openPromise;
  }

  get connectionState(): ConnectionState {
    return this.state;
  }

  async call(request: {
    method: string;
    params?: unknown;
    timeoutMs?: number;
    signal?: AbortSignal;
  }): Promise<unknown> {
    if (this.state !== "open" || !this.socket) {
      throw new TransportError(
        `call rejected: transport state is "${this.state}" (${this.url})`,
      );
    }
    if (request.signal?.aborted) {
      throw new TransportError(`call rejected: AbortSignal already aborted (${this.url})`);
    }

    const id = this.nextId++;
    const envelope: JsonRpcRequest = { jsonrpc: "2.0", id, method: request.method };
    if (request.params !== undefined) {
      envelope.params = request.params;
    }

    const timeoutMs = request.timeoutMs ?? this.callTimeoutMs;

    return new Promise<unknown>((resolve, reject) => {
      const pendingEntry: PendingCall = {
        resolve,
        reject,
        timeoutHandle: null,
        abortListener: null,
        signal: request.signal ?? null,
      };

      if (timeoutMs > 0) {
        pendingEntry.timeoutHandle = setTimeout(() => {
          if (this.pending.delete(id)) {
            this.detachAbort(pendingEntry);
            reject(
              new TimeoutError(
                `call "${request.method}" timed out after ${timeoutMs}ms (${this.url})`,
                timeoutMs,
              ),
            );
          }
        }, timeoutMs);
      }
      // timeoutMs <= 0 means "no client-side timeout": the call settles when the server replies
      // or when the transport closes (failAllPending rejects every entry). Used by ObjectHandle.invoke
      // so user-defined methods can take arbitrarily long without false TimeoutError rejections.

      if (request.signal) {
        pendingEntry.abortListener = () => {
          if (this.pending.delete(id)) {
            this.clearTimeoutHandle(pendingEntry);
            reject(
              new TransportError(
                `call "${request.method}" aborted via AbortSignal (${this.url})`,
              ),
            );
          }
        };
        request.signal.addEventListener("abort", pendingEntry.abortListener, { once: true });
      }

      this.pending.set(id, pendingEntry);

      try {
        this.socket!.send(JSON.stringify(envelope));
      } catch (sendErr) {
        if (this.pending.delete(id)) {
          this.clearTimeoutHandle(pendingEntry);
          this.detachAbort(pendingEntry);
          reject(
            new TransportError(`failed to send "${request.method}" (${this.url})`, {
              cause: sendErr,
            }),
          );
        }
      }
    });
  }

  onNotification(args: { method: string; handler: NotificationHandler }): CancelFn {
    let handlers = this.notificationHandlers.get(args.method);
    if (!handlers) {
      handlers = new Set();
      this.notificationHandlers.set(args.method, handlers);
    }
    handlers.add(args.handler);

    return makeIdempotent(() => {
      const set = this.notificationHandlers.get(args.method);
      if (!set) return;
      set.delete(args.handler);
      if (set.size === 0) {
        this.notificationHandlers.delete(args.method);
      }
    });
  }

  onDisconnect(handler: () => void): CancelFn {
    this.disconnectHandlers.add(handler);
    return makeIdempotent(() => {
      this.disconnectHandlers.delete(handler);
    });
  }

  onReconnect(handler: () => void): CancelFn {
    this.reconnectHandlers.add(handler);
    return makeIdempotent(() => {
      this.reconnectHandlers.delete(handler);
    });
  }

  onConnectionStateChange(handler: (state: ConnectionState) => void): CancelFn {
    this.connectionStateHandlers.add(handler);
    return makeIdempotent(() => {
      this.connectionStateHandlers.delete(handler);
    });
  }

  close(): void {
    if (this.state === "closed") return;
    const wasPreOpen = !this.hasEverConnected;
    this.setState("closed");
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    this.failAllPending(new TransportError(`transport closed (${this.url})`));
    if (this.socket) {
      try {
        this.socket.close();
      } catch {
        // ignore: best-effort
      }
      this.socket = null;
    }
    if (wasPreOpen) {
      this.rejectOpen(
        new TransportError(`transport closed before initial open (${this.url})`),
      );
    }
  }

  // -- internals -----------------------------------------------------------------------------

  private openSocket(): void {
    let socket: WebSocketLike;
    try {
      socket = this.factory(this.url);
    } catch (err) {
      if (!this.hasEverConnected && !this.retryInitialOpen) throw normalizeError(err);
      if (this.state !== "reconnecting") this.setState("reconnecting");
      this.scheduleReconnect();
      return;
    }
    this.socket = socket;

    socket.addEventListener("open", this.handleOpen);
    socket.addEventListener("message", this.handleMessage);
    socket.addEventListener("close", this.handleClose);
    socket.addEventListener("error", this.handleError);
  }

  private readonly handleOpen = (): void => {
    if (this.state === "closed") return;
    const wasReconnecting = this.state === "reconnecting";
    const isFirstOpen = !this.hasEverConnected;
    this.setState("open");
    this.reconnectAttempt = 0;
    this.hasEverConnected = true;
    if (wasReconnecting) {
      this.fireSnapshot(this.reconnectHandlers);
    }
    if (isFirstOpen) {
      this.resolveOpen();
    }
  };

  private readonly handleMessage = (event: { data: unknown }): void => {
    if (typeof event.data !== "string") return;
    let parsed: unknown;
    try {
      parsed = JSON.parse(event.data);
    } catch {
      return;
    }
    if (!parsed || typeof parsed !== "object") return;
    const message = parsed as Partial<JsonRpcSuccessResponse & JsonRpcErrorResponse & JsonRpcNotification>;

    if (typeof message.id === "number") {
      const entry = this.pending.get(message.id);
      if (!entry) return;
      this.pending.delete(message.id);
      this.clearTimeoutHandle(entry);
      this.detachAbort(entry);
      if ("error" in message && message.error) {
        entry.reject(new JsonRpcError(message.error.code, message.error.message, message.error.data));
      } else if ("result" in message) {
        entry.resolve(message.result);
      } else {
        entry.reject(new TransportError(`response for id ${message.id} has neither result nor error`));
      }
      return;
    }

    if (typeof message.method === "string") {
      const handlers = this.notificationHandlers.get(message.method);
      if (!handlers || handlers.size === 0) return;
      // Snapshot in case a handler cancels itself during dispatch.
      for (const handler of Array.from(handlers)) {
        try {
          handler(message.params);
        } catch (err) {
          this.reportError(normalizeError(err));
        }
      }
    }
  };

  private readonly handleClose = (_event: { code: number; reason: string }): void => {
    if (this.state === "closed") return;
    this.socket = null;
    const cause = this.lastWebSocketError ?? undefined;
    this.lastWebSocketError = null;
    const lostOpts: ErrorOptions = {};
    if (cause !== undefined) lostOpts.cause = cause;
    this.failAllPending(new TransportError(`connection lost (${this.url})`, lostOpts));
    const shouldRetry =
      this.reconnect.enabled && (this.hasEverConnected || this.retryInitialOpen);
    if (shouldRetry) {
      this.setState("reconnecting");
      if (this.hasEverConnected) {
        this.fireSnapshot(this.disconnectHandlers);
      }
      this.scheduleReconnect();
    } else {
      this.setState("closed");
      if (this.hasEverConnected) {
        this.fireSnapshot(this.disconnectHandlers);
      } else {
        const initOpts: ErrorOptions = {};
        if (cause !== undefined) initOpts.cause = cause;
        this.rejectOpen(new TransportError(`initial connect failed (${this.url})`, initOpts));
      }
    }
  };

  private readonly handleError = (event: unknown): void => {
    this.lastWebSocketError = event;
  };

  private scheduleReconnect(): void {
    if (this.state !== "reconnecting") return;
    const delay = Math.min(
      this.reconnect.initialDelayMs * Math.pow(this.reconnect.factor, this.reconnectAttempt),
      this.reconnect.maxDelayMs,
    );
    this.reconnectAttempt += 1;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      if (this.state !== "reconnecting") return;
      this.openSocket();
    }, delay);
  }

  private failAllPending(err: Error): void {
    for (const entry of this.pending.values()) {
      this.clearTimeoutHandle(entry);
      this.detachAbort(entry);
      entry.reject(err);
    }
    this.pending.clear();
  }

  private clearTimeoutHandle(entry: PendingCall): void {
    if (entry.timeoutHandle !== null) {
      clearTimeout(entry.timeoutHandle);
      entry.timeoutHandle = null;
    }
  }

  private detachAbort(entry: PendingCall): void {
    if (entry.signal && entry.abortListener) {
      entry.signal.removeEventListener("abort", entry.abortListener);
      entry.abortListener = null;
    }
  }

  private fireSnapshot(set: Set<() => void>): void {
    const snapshot = Array.from(set);
    for (const handler of snapshot) {
      try {
        handler();
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
  }

  private setState(next: ConnectionState): void {
    if (this.state === next) return;
    this.state = next;
    const snapshot = Array.from(this.connectionStateHandlers);
    for (const handler of snapshot) {
      try {
        handler(next);
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
  }
}
