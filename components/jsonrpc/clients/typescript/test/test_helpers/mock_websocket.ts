// === mock_websocket.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { WebSocketLike } from "../../src/internal/transport.js";

// Mirror native WebSocket.readyState values so consumers that depend on numeric ranges work.
const CONNECTING = 0;
const OPEN = 1;
const CLOSING = 2;
const CLOSED = 3;

type OpenListener = () => void;
type MessageListener = (event: { data: unknown }) => void;
type CloseListener = (event: { code: number; reason: string }) => void;
type ErrorListener = (event: unknown) => void;

/**
 * Test-controllable WebSocket double. Construction does NOT auto-open -- tests drive the
 * lifecycle by calling `simulateOpen()` etc. Capturing happens via `sentFrames`.
 *
 * `lastInstance` lets the test capture whichever mock the Transport's factory produced without
 * having to thread it through manually.
 */
export class MockWebSocket implements WebSocketLike {
  static lastInstance: MockWebSocket | null = null;

  readonly url: string;
  readyState: number = CONNECTING;

  readonly sentFrames: string[] = [];
  readonly closeCalls: { code: number | undefined; reason: string | undefined }[] = [];

  private readonly openListeners = new Set<OpenListener>();
  private readonly messageListeners = new Set<MessageListener>();
  private readonly closeListeners = new Set<CloseListener>();
  private readonly errorListeners = new Set<ErrorListener>();

  constructor(url: string) {
    this.url = url;
    MockWebSocket.lastInstance = this;
  }

  send(data: string): void {
    if (this.readyState !== OPEN) {
      throw new Error(`MockWebSocket: send while readyState=${this.readyState}`);
    }
    this.sentFrames.push(data);
  }

  close(code?: number, reason?: string): void {
    this.closeCalls.push({ code, reason });
    if (this.readyState === CLOSED) return;
    this.readyState = CLOSED;
    // Native WebSocket fires 'close' asynchronously even when close() is initiated locally; we
    // mirror that so the Transport's state machine sees a real handler invocation.
    queueMicrotask(() => this.fireClose(code ?? 1000, reason ?? ""));
  }

  addEventListener(type: "open", listener: OpenListener): void;
  addEventListener(type: "message", listener: MessageListener): void;
  addEventListener(type: "close", listener: CloseListener): void;
  addEventListener(type: "error", listener: ErrorListener): void;
  addEventListener(type: string, listener: unknown): void {
    switch (type) {
      case "open":
        this.openListeners.add(listener as OpenListener);
        break;
      case "message":
        this.messageListeners.add(listener as MessageListener);
        break;
      case "close":
        this.closeListeners.add(listener as CloseListener);
        break;
      case "error":
        this.errorListeners.add(listener as ErrorListener);
        break;
    }
  }

  // --- driver methods used by tests ----------------------------------------------------------

  simulateOpen(): void {
    this.readyState = OPEN;
    for (const listener of this.openListeners) listener();
  }

  simulateMessage(frame: string | object): void {
    const data = typeof frame === "string" ? frame : JSON.stringify(frame);
    for (const listener of this.messageListeners) listener({ data });
  }

  simulateBinaryMessage(): void {
    for (const listener of this.messageListeners) listener({ data: new Uint8Array([1, 2, 3]) });
  }

  simulateClose(code = 1006, reason = "abnormal"): void {
    if (this.readyState === CLOSED) return;
    this.readyState = CLOSED;
    this.fireClose(code, reason);
  }

  simulateError(): void {
    for (const listener of this.errorListeners) listener(new Error("simulated error"));
  }

  clearFrames(): void {
    this.sentFrames.length = 0;
  }

  private fireClose(code: number, reason: string): void {
    for (const listener of this.closeListeners) listener({ code, reason });
  }
}

/**
 * Helper: wait one macrotask. Lets queued microtasks settle before the test asserts. Useful
 * after `socket.send(...)` or `close()` which schedule via `queueMicrotask`.
 */
export function tick(ms = 0): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
