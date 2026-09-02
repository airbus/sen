// === client.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { ReportError } from "./connect.js";
import type { CustomTypeSpec, SessionInfo } from "./generated/index.js";
import { InterestHandle, type InterestDeclaration, type PreSubscription } from "./handles.js";
import type { JsonRpcClient } from "./internal/protocol_client.js";
import { normalizeError } from "./internal/report_error.js";
import { toWireSubscribeBlock } from "./internal/subscribe_codec.js";
import type { Transport } from "./internal/transport.js";
import type { TypeCache } from "./internal/type_cache.js";
import type { CancelFn } from "./values.js";

export type { SessionInfo };

/** Connection lifecycle states reported by {@link Client.connectionState} and {@link Client.onConnectionStateChange}. */
export type ConnectionState = "connecting" | "open" | "reconnecting" | "closed";

/** Client returned by `connect()`. Declare interests through it; per-object ops live on `ObjectHandle`. */
export class Client {
  private readonly cancelInterestUpdateListener: CancelFn;
  private readonly cancelPropertyChangedListener: CancelFn;
  private readonly cancelEventTriggeredListener: CancelFn;
  private readonly cancelTopologyChangedListener: CancelFn;
  private readonly cancelNotificationsDroppedListener: CancelFn;
  private readonly interestHandles = new Map<string, InterestHandle>();
  private readonly declareChains = new Map<string, Promise<void>>();
  private readonly topologyHandlers = new Set<(sessions: SessionInfo[]) => void>();
  private readonly notificationsDroppedHandlers = new Set<(count: number) => void>();
  private droppedNotificationCount = 0;
  private topologySubscribed = false;
  private lastTopology: SessionInfo[] = [];
  private hasTopology = false;
  private reestablishRun: Promise<void> | null = null;
  private reestablishQueued = false;
  /** Bumped on every reconnect; lets reestablishAll tell "queued during a pass on this same
   *  connection" (already satisfied) from "queued because the connection changed mid-pass". */
  private connectionEpoch = 0;

  /** @internal */
  constructor(
    private readonly transport: Transport,
    private readonly protocol: JsonRpcClient,
    private readonly typeCache: TypeCache,
    private readonly reportError: ReportError,
  ) {
    this.cancelInterestUpdateListener = this.protocol.onInterestUpdate((params) => {
      if (params.types.length > 0) this.typeCache.setMany(params.types);
      if (params.typeSchemas.length > 0) this.typeCache.setSchemasFromBlob(params.typeSchemas);
      this.interestHandles.get(params.interestName)?.applyUpdate(params);
    });
    this.cancelPropertyChangedListener = this.protocol.onPropertyChanged((params) => {
      this.interestHandles.get(params.interestName)?.dispatchPropertyChanged(params);
    });
    this.cancelEventTriggeredListener = this.protocol.onEventTriggered((params) => {
      this.interestHandles.get(params.interestName)?.dispatchEventTriggered(params);
    });
    this.cancelTopologyChangedListener = this.protocol.onTopologyChanged((params) => {
      this.lastTopology = params.sessions;
      this.hasTopology = true;
      for (const handler of this.topologyHandlers) handler(params.sessions);
    });

    // Registered unconditionally, not on first subscriber: the server sends this once when
    // backpressure clears and never repeats it, so a handler attached afterwards would learn
    // nothing. The running total is kept so a late subscriber can still discover that data
    // was missed.
    this.cancelNotificationsDroppedListener = this.protocol.onNotificationsDropped((params) => {
      this.droppedNotificationCount += params.count;
      for (const handler of this.notificationsDroppedHandlers) handler(params.count);
    });
    this.transport.onReconnect(() => {
      this.connectionEpoch += 1;
    });
  }

  /**
   * Declare an interest: a live query against the Sen object graph. The returned
   * {@link InterestHandle} starts out empty; matched objects arrive shortly after, either in
   * an immediate server-side push or in a subsequent `interestUpdate` notification (the wire
   * does not guarantee an order between the `createInterest` response and the initial push).
   * Register {@link InterestHandle.onObjectAdded} to handle them either way; it replays for
   * current matches and streams later arrivals. `name` must be unique for this connection.
   *
   * @example
   * ```ts
   * const interest = await client.declareInterest({
   *   name: "cats",
   *   query: "SELECT animals.Cat FROM my.tutorial",
   *   subscribe: { properties: "*" },  // optional pre-subscription
   * });
   * interest.onObjectAdded((cat) => console.log(cat.name));
   * // ... later
   * await interest.release();
   * ```
   */
  async declareInterest(params: {
    name: string;
    query: string;
    subscribe?: PreSubscription;
    /** Opt into JSON-Schema shipping: every interestUpdate for this interest's lifetime carries
     *  schema fragments for previously-unseen types in `params.typeSchemas`. Per-connection
     *  dedup applies. */
    withSchemas?: boolean;
    timeoutMs?: number;
    signal?: AbortSignal;
  }): Promise<InterestHandle> {
    const previous = this.declareChains.get(params.name);
    // Wait for any prior declare/release cycle for this name to fully settle before issuing
    // the next wire createInterest. `previous` is undefined for the first declare of a name,
    // or after the prior cycle has been cleared from the map.
    const chain: Promise<InterestHandle> = (async () => {
      if (previous !== undefined) {
        try {
          await previous;
        } catch {
          // A prior cycle's failure doesn't block subsequent declares; the handle that
          // failed is already torn down and the name is free for a fresh try.
        }
      }
      return this.declareInterestImpl(params);
    })();
    // Track the FULL lifecycle (declare -> use -> release) so the next declare for the same
    // name waits on it. A rejected declare unblocks immediately (no handle to release).
    const cycle: Promise<void> = chain.then(
      (h) => h.settled,
      () => undefined,
    );
    this.declareChains.set(params.name, cycle);
    void cycle.then(() => {
      // Clear the map entry once the cycle settles, so names that come and go don't pile up.
      // Guard against clobbering a newer chain that was installed during this cycle.
      if (this.declareChains.get(params.name) === cycle) {
        this.declareChains.delete(params.name);
      }
    });
    return chain;
  }

  private async declareInterestImpl(params: {
    name: string;
    query: string;
    subscribe?: PreSubscription;
    withSchemas?: boolean;
    timeoutMs?: number;
    signal?: AbortSignal;
  }): Promise<InterestHandle> {
    const wireSubscribe = toWireSubscribeBlock(params.subscribe);
    const declaration: InterestDeclaration = { query: params.query };
    if (wireSubscribe !== null) declaration.subscribe = wireSubscribe;
    if (params.withSchemas === true) declaration.withSchemas = true;
    const handle = new InterestHandle(
      params.name,
      declaration,
      this.protocol,
      this.typeCache,
      (interestName) => this.interestHandles.delete(interestName),
      this.reportError,
    );
    // Register before the wire call: the server may push the initial match-set immediately.
    this.interestHandles.set(params.name, handle);
    try {
      const call: Parameters<JsonRpcClient["createInterest"]>[0] = {
        interestName: params.name,
        query: params.query,
      };
      if (wireSubscribe !== null) call.subscribe = wireSubscribe;
      if (params.withSchemas === true) call.withSchemas = true;
      if (params.timeoutMs !== undefined) call.timeoutMs = params.timeoutMs;
      if (params.signal !== undefined) call.signal = params.signal;
      await this.protocol.createInterest(call);
      handle.markActive();
      handle.noteReestablished(this.connectionEpoch);
    } catch (err) {
      handle.markRejected(normalizeError(err));
      throw err;
    }
    return handle;
  }

  /** Current wire state. */
  get connectionState(): ConnectionState {
    return this.transport.connectionState;
  }

  /**
   * Read a `CustomTypeSpec` from the local cache, or `undefined` if not yet known. Types
   * arrive bundled on `interestUpdate` notifications when a class is first matched, so this
   * is typically populated by the time a consumer of an object asks for the class spec.
   * Synchronous; never round-trips.
   */
  getType(qualifiedName: string): CustomTypeSpec | undefined {
    return this.typeCache.get(qualifiedName);
  }

  /**
   * JSON-Schema fragment for a qualified type name from the local cache, or `undefined`
   * when the wire didn't ship one. Schemas arrive bundled on `interestUpdate.typeSchemas`
   * for interests opened with `withSchemas: true`, or via `fetchType(qual, { withSchema: true })`.
   * Synchronous; never round-trips.
   */
  getSchema(qualifiedName: string): object | undefined {
    return this.typeCache.getSchema(qualifiedName);
  }

  /**
   * Fetch a `CustomTypeSpec` (and optionally its JSON-Schema fragment) over the wire and
   * populate the local cache. Prefer the synchronous {@link getType} when the type is expected
   * to be cached already; this method is for cold lookups (no interest yet matched the class).
   *
   * With `withSchema: true`, the returned `schema` is the parsed JSON-Schema fragment, also
   * stored in the local cache so subsequent `getSchema(qual)` calls resolve synchronously.
   * Without it, `schema` is `undefined`.
   */
  async fetchType(
    qualifiedName: string,
    opts: { withSchema?: boolean; timeoutMs?: number; signal?: AbortSignal } = {},
  ): Promise<{ spec: CustomTypeSpec; schema?: object }> {
    const call: Parameters<JsonRpcClient["getType"]>[0] = { qualifiedName };
    if (opts.withSchema === true) call.withSchema = true;
    if (opts.timeoutMs !== undefined) call.timeoutMs = opts.timeoutMs;
    if (opts.signal !== undefined) call.signal = opts.signal;
    const result = await this.protocol.getType(call);
    this.typeCache.set(result.spec);
    const out: { spec: CustomTypeSpec; schema?: object } = { spec: result.spec };
    if (result.schema.length > 0) {
      const parsed = JSON.parse(result.schema) as object;
      this.typeCache.setSchema(qualifiedName, parsed);
      out.schema = parsed;
    }
    return out;
  }

  /** Currently-cached `CustomTypeSpec`s. Synchronous snapshot; never round-trips. */
  listKnownTypes(): CustomTypeSpec[] {
    return this.typeCache.values();
  }

  /** Fetch every qualified type name registered on the server, over the wire. Useful for cold
   *  type-browser surfaces; pair with {@link fetchType} to pull individual specs on demand. */
  async fetchTypeNames(opts: { timeoutMs?: number; signal?: AbortSignal } = {}): Promise<string[]> {
    return await this.protocol.getTypes(opts);
  }

  /**
   * Subscribe to type-cache growth. Fires once per `interestUpdate` batch that contained at
   * least one previously-unseen type. Useful for consumers (e.g., a UI hook that walked a
   * class inheritance chain and hit a missing parent) that need to retry their walk when the
   * cache may have learned new names. Returns a cancel function.
   *
   * Purely TS-side: the notification is fired from inside the wire-delivery handler after
   * the cache mutation completes; no wire-level event is involved.
   */
  onTypeAdded(handler: () => void): CancelFn {
    return this.typeCache.subscribe(handler);
  }

  /**
   * One-shot snapshot of the currently-detected sessions and their buses. Use
   * {@link onTopologyChanged} for a live view.
   */
  async listTopology(opts: { timeoutMs?: number; signal?: AbortSignal } = {}): Promise<SessionInfo[]> {
    return await this.protocol.listTopology(opts);
  }

  /**
   * Subscribe to topology updates. The handler is invoked with the current full snapshot once
   * a snapshot is available, then again on every session/bus add/remove. Multi-consumer: the
   * first call sends the underlying `subscribeTopology`; later calls share the same wire
   * subscription and the cached snapshot is replayed to new consumers immediately.
   *
   * The wire subscription stays open for the lifetime of the connection; there is no
   * `unsubscribe` half. Topology pushes are tiny and infrequent, and a permanent subscription
   * sidesteps the subscribe / unsubscribe / subscribe race that would otherwise need
   * serialization.
   *
   * @example
   * ```ts
   * const cancel = client.onTopologyChanged((sessions) => {
   *   for (const s of sessions) console.log(s.name, s.buses);
   * });
   * // ... later
   * cancel();
   * ```
   */
  onTopologyChanged(
    handler: (sessions: SessionInfo[]) => void,
    opts: { signal?: AbortSignal } = {},
  ): CancelFn {
    this.topologyHandlers.add(handler);
    if (!this.topologySubscribed) {
      this.topologySubscribed = true;
      this.protocol.subscribeTopology().catch((err) => this.reportError(normalizeError(err)));
    } else if (this.hasTopology) {
      handler(this.lastTopology);
    }
    const cancel = (): void => {
      this.topologyHandlers.delete(handler);
    };
    opts.signal?.addEventListener("abort", cancel, { once: true });
    return cancel;
  }

  /** Close the connection. The client cannot be reused afterward. */
  close(): void {
    this.cancelInterestUpdateListener();
    this.cancelPropertyChangedListener();
    this.cancelEventTriggeredListener();
    this.cancelTopologyChangedListener();
    this.cancelNotificationsDroppedListener();
    this.topologyHandlers.clear();
    this.notificationsDroppedHandlers.clear();
    this.interestHandles.clear();
    this.typeCache.clearListeners();
    this.transport.close();
  }

  /**
   * Notified when the connection drops unexpectedly (not via `close()`). Interests and
   * subscriptions are invalidated; while `reconnect.autoReestablish` is on (the default)
   * they re-declare automatically after the reconnect - wire these handlers for UI state or
   * custom recovery work, not to call {@link reestablishAll} again.
   *
   * @example
   * ```ts
   * client.onDisconnect(() => console.warn("offline"));
   * client.onReconnect(() => console.info("back online"));
   * ```
   */
  onDisconnect(handler: () => void): CancelFn {
    return this.transport.onDisconnect(handler);
  }

  /** Notified after a successful reconnect. */
  onReconnect(handler: () => void): CancelFn {
    return this.transport.onReconnect(handler);
  }

  /**
   * Notified on every connection-state transition with the new state. Convenient for a single
   * UI indicator that reacts to all transitions; for one-off reconnect work see
   * {@link onReconnect}. Does not replay the current state on register: read
   * {@link connectionState} once if you need the starting value.
   *
   * @example
   * ```ts
   * client.onConnectionStateChange((state) => {
   *   statusEl.textContent = state;
   * });
   * ```
   */
  onConnectionStateChange(handler: (state: ConnectionState) => void): CancelFn {
    return this.transport.onConnectionStateChange(handler);
  }

  /**
   * Total notifications the server has reported dropping on this connection since it opened.
   *
   * The server drops unreliable notifications while its outbound buffer is above the high
   * watermark, and reports the tally once the buffer drains. A non-zero value means this
   * client's cached object state is incomplete: some property changes and events never
   * arrived, and there is no record of which. It does not decrease.
   */
  get droppedNotifications(): number {
    return this.droppedNotificationCount;
  }

  /**
   * Subscribe to backpressure drop reports. The handler receives the count for each window,
   * not the running total; use {@link Client.droppedNotifications} for that.
   *
   * A client that receives this cannot tell which updates it missed, only that it missed
   * some. Waiting for the next change is not a recovery: a property that stopped moving will
   * never send another. Re-read what you display instead, through
   * `InterestHandle.getObjectsBatchState()`.
   *
   * @example
   * ```ts
   * client.onNotificationsDropped((count) => {
   *   console.warn(`missed ${count} updates; refreshing`);
   *   void refreshVisiblePanels();
   * });
   * ```
   */
  onNotificationsDropped(handler: (count: number) => void): CancelFn {
    this.notificationsDroppedHandlers.add(handler);
    return () => {
      this.notificationsDroppedHandlers.delete(handler);
    };
  }

  /**
   * Re-declare every declared interest, in parallel. Subscriptions resume automatically once
   * interests become active again. Best-effort: per-interest failures surface through
   * `onError` but don't reject the promise. Typically called from an `onReconnect` handler
   * (which is wired automatically when `reconnect.autoReestablish` is on, the default).
   *
   * Resolves once `createInterest` completes; per-property re-subscribes are then fired
   * asynchronously from the next `interestUpdate`. Callers needing subscriptions provably
   * live should await the next `propertyChanged` or round-trip via `getProperty`.
   *
   * No-op (resolves immediately) when the transport isn't currently `open`. This avoids a
   * spurious wave of `TransportError` reports when called during reconnect; the auto-wiring
   * fires again on the eventual `onReconnect` and does the real work then.
   *
   * Single-flight: overlapping calls (the `autoReestablish` wiring racing a manual call, or
   * two reconnects in quick succession) coalesce into the run already in progress, plus at
   * most one trailing rerun when the connection changed mid-pass or a re-declare failed.
   * Interleaved runs used to double-issue `createInterest` for the same names; the loser's
   * "interest already exists" rejection then left its handle cleared and permanently empty.
   */
  async reestablishAll(): Promise<void> {
    if (this.reestablishRun) {
      this.reestablishQueued = true;
      return this.reestablishRun;
    }
    this.reestablishRun = (async () => {
      do {
        this.reestablishQueued = false;
        if (this.transport.connectionState !== "open") return;
        const epochAtStart = this.connectionEpoch;
        // Don't clear the typeCache: the server re-bundles each interest's `types` on the next
        // interestUpdate, and clearing first opens a window where parseVar throws for still-active
        // interests' property pushes.
        const results = await Promise.allSettled(
          Array.from(this.interestHandles.values()).map((h) => h.reestablish(epochAtStart)),
        );
        let anyRejected = false;
        for (const result of results) {
          if (result.status === "rejected") {
            anyRejected = true;
            this.reportError(normalizeError(result.reason));
          }
        }
        // A rerun queued while this pass ran on the same, still-healthy connection is already
        // satisfied by the pass itself; rerunning would re-issue createInterest into
        // "interest already exists".
        if (this.reestablishQueued && this.connectionEpoch === epochAtStart && !anyRejected) {
          this.reestablishQueued = false;
        }
      } while (this.reestablishQueued);
    })();
    try {
      await this.reestablishRun;
    } finally {
      this.reestablishRun = null;
    }
  }
}
