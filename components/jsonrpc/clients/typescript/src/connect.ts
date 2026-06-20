// === connect.ts ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { Client } from "./client.js";
import { TransportError } from "./errors.js";
import { JsonRpcClient } from "./internal/protocol_client.js";
import { defaultReportError } from "./internal/report_error.js";
import { Transport, type WebSocketFactory } from "./internal/transport.js";
import { TypeCache } from "./internal/type_cache.js";

const defaultOpenTimeoutMs = 2_000;

/** Receives errors the library catches in background handlers; passed via {@link ClientOptions.onError}. */
export type ReportError = (err: Error) => void;

/**
 * Auto-reconnect tuning for {@link ClientOptions.reconnect}. Defaults: `enabled: true`,
 * `initialDelayMs: 100`, `maxDelayMs: 5000`, `factor: 2` (exponential backoff capped at
 * `maxDelayMs`), `autoReestablish: true` (run `client.reestablishAll()` automatically after
 * each successful reconnect).
 */
export interface ReconnectOptions {
  enabled?: boolean;
  initialDelayMs?: number;
  maxDelayMs?: number;
  factor?: number;
  /** Re-issue every active `declareInterest` (which re-arms its property/event subscriptions)
   *  after each successful reconnect. Default `true`. Set `false` for custom recovery; the
   *  client still fires `onReconnect` so consumers can do their own work. */
  autoReestablish?: boolean;
}

export interface ClientOptions {
  /** WebSocket URL (e.g. `ws://localhost:8080`). */
  url: string;

  /** Per-call timeout default. */
  defaultTimeoutMs?: number;

  /** Time to wait for the first connection before giving up. Defaults to 2 seconds. With
   *  `initialReconnect: true`, this is the total deadline across all retries (not per try). */
  openTimeoutMs?: number;

  /** When `true`, retry the initial connect using the same backoff schedule as
   *  {@link ReconnectOptions} until `openTimeoutMs` elapses. Default `false` (the initial
   *  connect is one-shot). Set `true` for UIs that should start and wait for Sen to come
   *  up; leave default for short-lived scripts where one-shot semantics are clearer. */
  initialReconnect?: boolean;

  /** Auto-reconnect behavior; see {@link ReconnectOptions} for fields and defaults. */
  reconnect?: ReconnectOptions;

  /** Receives errors the library catches in background handlers. Defaults to `console.error`. */
  onError?: ReportError;

  /** @internal */
  webSocketFactory?: WebSocketFactory;
}

/**
 * Connect to a Sen JSON-RPC gateway. Resolves once the WebSocket is open and ready for calls.
 * The returned `Client` owns the connection; close it with `client.close()` when done.
 *
 * @example
 * ```ts
 * const client = await connect({ url: "ws://127.0.0.1:8080" });
 * try {
 *   const interest = await client.declareInterest({ name: "all", query: "SELECT * FROM bus" });
 *   // ...
 *   await interest.release();
 * } finally {
 *   client.close();
 * }
 * ```
 */
export async function connect(options: ClientOptions): Promise<Client> {
  const reportError = options.onError ?? defaultReportError;
  const transportOpts: ConstructorParameters<typeof Transport>[0] = {
    url: options.url,
    reportError,
  };
  if (options.defaultTimeoutMs !== undefined) transportOpts.defaultTimeoutMs = options.defaultTimeoutMs;
  if (options.reconnect !== undefined) transportOpts.reconnect = options.reconnect;
  if (options.webSocketFactory !== undefined) transportOpts.webSocketFactory = options.webSocketFactory;
  if (options.initialReconnect === true) transportOpts.retryInitialOpen = true;
  const transport = new Transport(transportOpts);

  const openTimeoutMs = options.openTimeoutMs ?? defaultOpenTimeoutMs;
  let timeoutHandle: ReturnType<typeof setTimeout> | null = null;
  const timeoutPromise = new Promise<never>((_resolve, reject) => {
    timeoutHandle = setTimeout(() => {
      reject(new TransportError(`connect timed out after ${openTimeoutMs}ms (${options.url})`));
    }, openTimeoutMs);
  });

  try {
    await Promise.race([transport.whenOpen(), timeoutPromise]);
  } catch (err) {
    transport.close();
    throw err;
  } finally {
    if (timeoutHandle !== null) clearTimeout(timeoutHandle);
  }

  const protocol = new JsonRpcClient(transport, reportError);
  const typeCache = new TypeCache(reportError);
  const client = new Client(transport, protocol, typeCache, reportError);

  const reconnectEnabled = options.reconnect?.enabled ?? true;
  const autoReestablish = options.reconnect?.autoReestablish ?? true;
  if (reconnectEnabled && autoReestablish) {
    client.onReconnect(() => {
      client.reestablishAll().catch((err: unknown) => {
        reportError(err instanceof Error ? err : new Error(String(err)));
      });
    });
  }

  return client;
}
