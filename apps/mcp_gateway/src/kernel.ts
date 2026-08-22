// === kernel.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { setMaxListeners } from "node:events";
import { connect, type CancelFn, type Client, type ConnectionState, type InterestHandle, type Var } from "@sen/client";
import { GatewayStateError, KernelDisconnectedError } from "./errors.js";
import { RingBuffer } from "./util/ring_buffer.js";

export interface BufferedEvent {
  objectName: string;
  eventName: string;
  args: Var[];
  /** RFC 3339 with nanosecond fraction; server-side timestamp. */
  timestamp: string;
}

export const EVENT_BUFFER_CAP = 1000;

// An entry can hold a Var[] of any size, so counting them bounds nothing. Whichever limit is
// hit first wins.
export const EVENT_BUFFER_BYTE_CAP = 8 * 1024 * 1024;

export const INTEREST_CAP_PER_KERNEL = 64;

// An AbortSignal never warns about its listener count until something sets a ceiling, so this
// call is the only thing that would report a reintroduced leak.
//
// 1024 is far above anything real. @sen/client attaches one listener per live subscription and
// one per in-flight call, and subscriptions are per interest per event name, so a busy
// deployment sits in the low hundreds. Set it near that and the first false warning teaches
// whoever reads it to ignore the next one.
const ABORT_LISTENER_CEILING = 1024;

export class Kernel {
  private clientPromise: Promise<Client> | null = null;
  private latestState: ConnectionState = "connecting";
  private isShuttingDown = false;
  private readonly abortController = new AbortController();
  readonly interests = new Map<string, InterestHandle>();
  // Atomic reservation against concurrent declareInterest: two parallel callers see each
  // other in pendingInterests and one hits the duplicate-name / cap guard instead of both
  // passing the check and leaking the loser's handle.
  private readonly pendingInterests = new Set<string>();
  private readonly eventBuffers = new Map<string, RingBuffer<BufferedEvent>>();
  // NUL-separated (interest, object, event) key; no character collision risk.
  private readonly eventCancels = new Map<string, CancelFn>();
  private readonly eventSubscribes = new Map<string, Promise<void>>();

  constructor(
    readonly name: string,
    readonly url: string,
    private readonly openTimeoutMs: number,
    private readonly log: (msg: string) => void,
  ) {
    setMaxListeners(ABORT_LISTENER_CEILING, this.abortController.signal);
  }

  get state(): ConnectionState {
    return this.latestState;
  }

  get signal(): AbortSignal {
    return this.abortController.signal;
  }

  assertOpen(): void {
    if (this.isShuttingDown) throw new KernelDisconnectedError(this.name);
  }

  reserveInterestSlot(name: string): void {
    if (this.interests.has(name) || this.pendingInterests.has(name)) {
      throw new GatewayStateError(`interest already open with name: ${name}`);
    }
    if (this.interests.size + this.pendingInterests.size >= INTEREST_CAP_PER_KERNEL) {
      throw new GatewayStateError(
        `interest count limit reached (${INTEREST_CAP_PER_KERNEL}); ` +
          `release one with releaseInterest before declaring a new one`,
      );
    }
    this.pendingInterests.add(name);
  }

  commitInterest(name: string, handle: InterestHandle): void {
    this.pendingInterests.delete(name);
    this.interests.set(name, handle);
  }

  releaseInterestSlot(name: string): void {
    this.pendingInterests.delete(name);
  }

  async openWs(): Promise<void> {
    await this.client();
  }

  client(): Promise<Client> {
    // A failed dial nulls clientPromise, so without this a call landing after shutdown would
    // open a socket nobody owns and nothing closes.
    if (this.isShuttingDown) return Promise.reject(new KernelDisconnectedError(this.name));
    if (this.clientPromise === null) {
      this.clientPromise = connect({
        url: this.url,
        openTimeoutMs: this.openTimeoutMs,
        onError: (err) => this.log(`[${this.name}] client error: ${err.message}`),
      })
        .then((c) => {
          this.latestState = c.connectionState;
          c.onConnectionStateChange((s) => {
            this.latestState = s;
          });
          // Transport auto-reconnects; interests/subscriptions don't, so re-establish here.
          c.onReconnect(() => {
            c.reestablishAll().catch((err: unknown) => {
              this.log(`[${this.name}] reestablishAll failed: ${err instanceof Error ? err.message : String(err)}`);
            });
          });
          c.onDisconnect(() => this.log(`[${this.name}] connection dropped; auto-reconnect in progress`));
          return c;
        })
        .catch((err: unknown) => {
          this.clientPromise = null;
          this.latestState = "closed";
          throw err;
        });
    }
    return this.clientPromise;
  }

  async shutdown(): Promise<void> {
    if (this.isShuttingDown) return;
    this.isShuttingDown = true;
    // Abort BEFORE any await so in-flight RPCs reject promptly via @sen/client's signal
    // propagation (pending calls reject with TransportError).
    this.abortController.abort();
    if (this.clientPromise === null) return;
    let client: Client;
    try {
      client = await this.clientPromise;
    } catch {
      return;
    }
    for (const interestName of this.interests.keys()) this.tearDownInterestEvents(interestName);
    await Promise.allSettled(
      Array.from(this.interests.values()).map((h) => h.release()),
    );
    this.interests.clear();
    client.close();
  }

  async subscribeToEvent(interestName: string, objectName: string, eventName: string): Promise<boolean> {
    const handle = this.interests.get(interestName);
    if (handle === undefined) throw new Error(`no interest open with name: ${interestName}`);
    const obj = handle.objectByName(objectName);
    if (obj === undefined) {
      throw new Error(`object not in interest match-set: ${objectName} (interest=${interestName})`);
    }
    const key = eventKey(interestName, objectName, eventName);
    // Ride along with a subscribe already in flight instead of installing a second handler,
    // then look again: an unsubscribe or a shutdown during that wait leaves nothing subscribed,
    // and answering "already subscribed" would strand the caller polling a buffer no one fills.
    for (let pending = this.eventSubscribes.get(key); pending !== undefined; ) {
      await pending;
      if (this.eventCancels.has(key)) return false;
      // releaseInterest drops the interest before it awaits, so the handle captured above may
      // now belong to nothing.
      this.assertOpen();
      if (this.interests.get(interestName) !== handle) {
        throw new Error(`no interest open with name: ${interestName}`);
      }
      pending = this.eventSubscribes.get(key);
    }
    if (this.eventCancels.has(key)) return false;
    const cancel = obj.onEventTriggered(
      eventName,
      (args, info) => {
        let buffer = this.eventBuffers.get(interestName);
        if (buffer === undefined) {
          buffer = new RingBuffer<BufferedEvent>(EVENT_BUFFER_CAP, {
            maxBytes: EVENT_BUFFER_BYTE_CAP,
            sizeOf: approxEventBytes,
          });
          this.eventBuffers.set(interestName, buffer);
        }
        buffer.push({ objectName, eventName, args, timestamp: info.timestamp });
      },
      { signal: this.abortController.signal },
    );
    this.eventCancels.set(key, cancel);
    const ack = obj.awaitEventSubscribed(eventName);
    // Riders wait on this rather than on the raw ack, so by the time one wakes the cleanup below
    // has run and the maps are truthful. It never rejects; a rider that finds nothing subscribed
    // retries and reports its own failure.
    const { promise: outcome, settle } = settleable();
    this.eventSubscribes.set(key, outcome);
    // awaitEventSubscribed doesn't accept a signal upstream; race manually so shutdown
    // during the wait rejects promptly instead of hanging on the closing socket.
    const aborted = abortRace(this.abortController.signal);
    let succeeded = false;
    try {
      await Promise.race([ack, aborted.promise]);
      succeeded = true;
    } finally {
      aborted.detach();
      // Only ever retract our own entries: an unsubscribe followed by a fresh subscribe during
      // the ack wait installs a different handler under this key.
      if (this.eventSubscribes.get(key) === outcome) this.eventSubscribes.delete(key);
      if (!succeeded) {
        cancel();
        if (this.eventCancels.get(key) === cancel) this.eventCancels.delete(key);
      }
      settle();
    }
    return true;
  }

  unsubscribeFromEvent(interestName: string, objectName: string, eventName: string): boolean {
    const key = eventKey(interestName, objectName, eventName);
    const cancel = this.eventCancels.get(key);
    if (cancel === undefined) return false;
    cancel();
    this.eventCancels.delete(key);
    // The subscribe whose ack is still in flight is now cancelled; leaving its entry behind
    // would make the next subscriber wait on it and then report a subscription that is gone.
    this.eventSubscribes.delete(key);
    return true;
  }

  unsubscribeAllEventsForInterest(interestName: string): number {
    const prefix = `${interestName}\x00`;
    let count = 0;
    for (const [key, cancel] of this.eventCancels) {
      if (key.startsWith(prefix)) {
        cancel();
        this.eventCancels.delete(key);
        this.eventSubscribes.delete(key);
        count++;
      }
    }
    return count;
  }

  drainEvents(interestName: string): BufferedEvent[] {
    const buffer = this.eventBuffers.get(interestName);
    return buffer === undefined ? [] : buffer.drainAll();
  }

  tearDownInterestEvents(interestName: string): void {
    const prefix = `${interestName}\x00`;
    for (const [key, cancel] of this.eventCancels) {
      if (key.startsWith(prefix)) {
        cancel();
        this.eventCancels.delete(key);
        this.eventSubscribes.delete(key);
      }
    }
    this.eventBuffers.delete(interestName);
  }
}

function eventKey(interestName: string, objectName: string, eventName: string): string {
  return `${interestName}\x00${objectName}\x00${eventName}`;
}

function settleable(): { promise: Promise<void>; settle: () => void } {
  let settle!: () => void;
  const promise = new Promise<void>((resolve) => {
    settle = resolve;
  });
  return { promise, settle };
}

// Rejects when `signal` fires. The ack usually wins the race and the signal lives as long as
// the kernel, so the caller must detach(): one retained closure per subscribe accumulates on
// a path clients take repeatedly.
function abortRace(signal: AbortSignal): { promise: Promise<never>; detach: () => void } {
  let detach = (): void => undefined;
  const promise = new Promise<never>((_, reject) => {
    const onAbort = (): void => reject(new KernelDisconnectedError("(kernel shut down)"));
    if (signal.aborted) {
      onAbort();
      return;
    }
    signal.addEventListener("abort", onAbort, { once: true });
    detach = (): void => signal.removeEventListener("abort", onAbort);
  });
  return { promise, detach };
}

// Flat charge for the header of a heap object, a string or a property slot.
const NODE_OVERHEAD_BYTES = 64;

// Past either limit the entry is already too big to keep whole, so counting on would only cost
// time. The node budget is also what keeps the walk safe against a self-referential value.
const WALK_NODE_BUDGET = 10_000;

// Rough retained size of one buffered event. Runs on every delivery, and the buffer only has to
// notice a megabyte-sized payload, so precision would be wasted here.
function approxEventBytes(event: BufferedEvent): number {
  let total = 2 * (event.objectName.length + event.eventName.length + event.timestamp.length);
  for (const arg of event.args) total += approxVarBytes(arg);
  return total + NODE_OVERHEAD_BYTES;
}

function approxVarBytes(value: Var): number {
  let total = 0;
  const stack: Var[] = [value];
  for (let visited = 0; stack.length > 0; visited++) {
    if (visited >= WALK_NODE_BUDGET || total >= EVENT_BUFFER_BYTE_CAP) break;
    const node = stack.pop()!;
    switch (typeof node) {
      case "string":
        total += NODE_OVERHEAD_BYTES + 2 * node.length;
        break;
      case "bigint":
        total += 16;
        break;
      case "object":
        if (node === null) {
          total += 8;
        } else if (Array.isArray(node)) {
          total += NODE_OVERHEAD_BYTES + 8 * node.length;
          for (const item of node) stack.push(item);
        } else {
          // Plain record, Variant or Quantity: all expose their payload as own properties.
          const fields = node as Record<string, Var>;
          total += NODE_OVERHEAD_BYTES;
          for (const key of Object.keys(fields)) {
            total += NODE_OVERHEAD_BYTES + 2 * key.length;
            stack.push(fields[key]!);
          }
        }
        break;
      default:
        total += 8;
    }
  }
  return total;
}
