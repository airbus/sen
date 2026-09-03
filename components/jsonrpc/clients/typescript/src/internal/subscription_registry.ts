// === subscription_registry.ts ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { ReportError } from "../connect.js";
import { TransportError } from "../errors.js";
import { parseVar } from "./var_codec.js";
import type { JsonRpcClient } from "./protocol_client.js";
import { normalizeError } from "./report_error.js";
import type { TypeCache } from "./type_cache.js";
import type { PropertyChangedNotification, PropertyValueList } from "../generated/index.js";
import type {
  AnyChangeHandler,
  DeliveryInfo,
  EventTriggeredHandler,
  PropertyChangedHandler,
  SubscribeOptions,
} from "../handles.js";
import type { CancelFn, Var } from "../values.js";
import { findEventInClass, findPropertyInClass, getClassSpec } from "./class_spec_lookup.js";

interface Consumer<H> {
  handler: H;
  cancelled: boolean;
  detachSignal?: () => void;
}

interface ConsumerSet<H> {
  consumers: Set<Consumer<H>>;
  wireSent: boolean;
  /** Resolves when the most recent first-subscribe wire frame has been ack'd by the server.
   *  Callers that need "subscribe-then-trigger" semantics (e.g., subscribe to an event then
   *  fire a method that triggers it) can await this to avoid racing the subscribe RPC. */
  wireReady: Promise<void> | undefined;
}

function newConsumerSet<H>(): ConsumerSet<H> {
  return { consumers: new Set(), wireSent: false, wireReady: undefined };
}

/**
 * Per-object subscription state. One wire subscribe per (object, member) regardless of how many
 * consumers register; refcounted by `consumers.size`. Survives object remove/add cycles within
 * the interest so subscribers keep firing across the gap.
 */
export class ObjectSubscriptions {
  private readonly properties = new Map<string, ConsumerSet<PropertyChangedHandler>>();
  private readonly events = new Map<string, ConsumerSet<EventTriggeredHandler>>();
  private readonly anyChange: ConsumerSet<AnyChangeHandler> = newConsumerSet();
  /** Last-known value + timestamp per property. Populated from propertyChanged dispatches and
   *  from interestUpdate `currentValues` seeds. Used to replay to late subscribers so a
   *  property that won't change again (static) or whose wire subscription was already live
   *  (server skips its seed) still delivers a value to every consumer. */
  private readonly valueCache = new Map<string, { value: Var; timestamp: string }>();
  private maxRateHz: number | null = null;

  constructor(
    private readonly protocol: JsonRpcClient,
    private readonly typeCache: TypeCache,
    private readonly interestName: string,
    private readonly objectName: string,
    readonly qualifiedClassName: string,
    private readonly reportError: ReportError,
  ) {}

  onPropertyChanged(
    propertyName: string,
    handler: PropertyChangedHandler,
    opts: SubscribeOptions = {},
  ): CancelFn {
    let set = this.properties.get(propertyName);
    if (!set) {
      set = newConsumerSet();
      this.properties.set(propertyName, set);
    }
    const cancel = this.subscribe(
      set,
      handler,
      opts,
      () => this.firePropertySubscribe(propertyName),
      () => {
        this.properties.delete(propertyName);
        // No subscribeAll consumer means no wire path is keeping `propertyName`'s value
        // fresh once we drop the per-property wire sub. Evict so a future resubscribe
        // waits for the next server push instead of replaying a stale snapshot.
        if (this.anyChange.consumers.size === 0) {
          this.invalidateCachedValue(propertyName);
        }
        return this.firePropertyUnsubscribe(propertyName);
      },
    );
    this.replayCachedTo(propertyName, handler);
    return cancel;
  }

  onEventTriggered(
    eventName: string,
    handler: EventTriggeredHandler,
    opts: SubscribeOptions = {},
  ): CancelFn {
    let set = this.events.get(eventName);
    if (!set) {
      set = newConsumerSet();
      this.events.set(eventName, set);
    }
    return this.subscribe(
      set,
      handler,
      opts,
      () => this.fireEventSubscribe(eventName),
      () => {
        this.events.delete(eventName);
        return this.fireEventUnsubscribe(eventName);
      },
    );
  }

  /** Resolves once the server has ack'd the most recent first-subscribe for `eventName`.
   *  No-op if no one is currently subscribed. Use after onEventTriggered when you need
   *  subscribe-then-trigger semantics (e.g., a method invocation that fires the event). */
  awaitEventSubscribed(eventName: string): Promise<void> {
    return this.events.get(eventName)?.wireReady ?? Promise.resolve();
  }

  /** Symmetric partner to {@link awaitEventSubscribed}: resolves once the server has ack'd the
   *  property's first-subscribe, or rejects with the wire error. No-op if no one is currently
   *  subscribed. A typo'd property name otherwise only surfaces via `reportError`. */
  awaitPropertySubscribed(propertyName: string): Promise<void> {
    return this.properties.get(propertyName)?.wireReady ?? Promise.resolve();
  }

  onAnyChange(handler: AnyChangeHandler, opts: SubscribeOptions = {}): CancelFn {
    const cancel = this.subscribe(
      this.anyChange,
      handler,
      opts,
      () => this.fireSubscribeAll(),
      () => {
        // Drop any cached value that was only being kept fresh by subscribeAll. Entries
        // that still have a per-property consumer stay; the per-property wire sub keeps
        // them fresh.
        for (const propertyName of Array.from(this.valueCache.keys())) {
          const propSet = this.properties.get(propertyName);
          if (!propSet || propSet.consumers.size === 0) {
            this.invalidateCachedValue(propertyName);
          }
        }
        return this.fireUnsubscribeAll();
      },
    );
    this.replayCachedBundleTo(handler);
    return cancel;
  }

  /** Cache update is unconditional: onAnyChange callers may read it without a per-property sub. */
  dispatchPropertyChanged(params: PropertyChangedNotification): void {
    let classSpec;
    try {
      classSpec = getClassSpec(this.qualifiedClassName, this.typeCache);
    } catch (err) {
      this.reportError(normalizeError(err));
      return;
    }
    const info: DeliveryInfo = { timestamp: params.timestamp };
    const bundleMap = this.anyChange.consumers.size > 0 ? new Map<string, Var>() : null;
    for (const pair of params.values) {
      try {
        const propertySpec = findPropertyInClass(classSpec, pair.propertyName, this.typeCache);
        if (!propertySpec) {
          throw new TransportError(
            `propertyChanged dispatch: "${pair.propertyName}" not found on class "${this.qualifiedClassName}"`,
          );
        }
        const value = parseVar(propertySpec.type, JSON.parse(pair.value), this.typeCache);
        this.valueCache.set(pair.propertyName, { value, timestamp: params.timestamp });
        if (bundleMap) bundleMap.set(pair.propertyName, value);
        const set = this.properties.get(pair.propertyName);
        if (set && set.consumers.size > 0) {
          this.fireSet(set, (h) => h(value, info));
        }
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
    if (bundleMap) {
      this.fireSet(this.anyChange, (h) => h(bundleMap, info));
    }
  }

  dispatchEventTriggered(eventName: string, argsJson: string, info: { timestamp: string }): void {
    const set = this.events.get(eventName);
    if (!set || set.consumers.size === 0) return;
    try {
      const classSpec = getClassSpec(this.qualifiedClassName, this.typeCache);
      const eventSpec = findEventInClass(classSpec, eventName, this.typeCache);
      if (!eventSpec) {
        throw new TransportError(
          `eventTriggered dispatch: "${eventName}" not found on class "${this.qualifiedClassName}"`,
        );
      }
      const rawArgs = JSON.parse(argsJson) as unknown[];
      const parsed = eventSpec.args.map((arg, i) =>
        parseVar(arg.type, rawArgs[i], this.typeCache),
      );
      this.fireSet(set, (h) => h(parsed, info));
    } catch (err) {
      this.reportError(normalizeError(err));
    }
  }

  seedCurrentValues(values: PropertyValueList): void {
    if (values.length === 0) return;
    let classSpec;
    try {
      classSpec = getClassSpec(this.qualifiedClassName, this.typeCache);
    } catch (err) {
      this.reportError(normalizeError(err));
      return;
    }
    for (const pair of values) {
      try {
        const propertySpec = findPropertyInClass(classSpec, pair.propertyName, this.typeCache);
        if (!propertySpec) {
          throw new TransportError(
            `currentValues seed: "${pair.propertyName}" not found on class "${this.qualifiedClassName}"`,
          );
        }
        const value = parseVar(propertySpec.type, JSON.parse(pair.value), this.typeCache);
        // No per-value timestamp on interestUpdate.currentValues; empty string is a sentinel
        // that subsequent propertyChanged dispatches will overwrite.
        this.valueCache.set(pair.propertyName, { value, timestamp: "" });
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
  }

  getCachedValue(propertyName: string): Var | undefined {
    return this.valueCache.get(propertyName)?.value;
  }

  setCachedValue(propertyName: string, value: Var): void {
    const existing = this.valueCache.get(propertyName);
    this.valueCache.set(propertyName, { value, timestamp: existing?.timestamp ?? "" });
  }

  clearValueCache(): void {
    this.valueCache.clear();
  }

  invalidateCachedValue(propertyName: string): void {
    this.valueCache.delete(propertyName);
  }

  /**
   * Asynchronously deliver the cached value for one property to a single new consumer.
   * Done as a microtask so the consumer's `cancel` is bound before the handler can run and
   * so the call site sees the same "subscribe returns, then handler fires" ordering it gets
   * for server-sent values.
   */
  private replayCachedTo(propertyName: string, handler: PropertyChangedHandler): void {
    const cached = this.valueCache.get(propertyName);
    if (!cached) return;
    const value = cached.value;
    const info: DeliveryInfo = { timestamp: cached.timestamp };
    queueMicrotask(() => {
      try {
        handler(value, info);
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    });
  }

  /** Same idea for `onAnyChange`: replay every cached property as one bundle. */
  private replayCachedBundleTo(handler: AnyChangeHandler): void {
    if (this.valueCache.size === 0) return;
    const bundle = new Map<string, Var>();
    let latest = "";
    for (const [name, entry] of this.valueCache) {
      bundle.set(name, entry.value);
      if (entry.timestamp > latest) latest = entry.timestamp;
    }
    const info: DeliveryInfo = { timestamp: latest };
    queueMicrotask(() => {
      try {
        handler(bundle, info);
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    });
  }

  /**
   * Refresh every currently-active subscribe with the new rate so the server's interval tracks
   * the latest ask. Stored on the instance so a later first-subscribe also picks it up.
   */
  async setMaxRateHz(hz: number | null): Promise<void> {
    this.maxRateHz = hz;
    const refreshes: Promise<void>[] = [];
    for (const [name, set] of this.properties) {
      if (set.consumers.size > 0) refreshes.push(this.firePropertySubscribe(name));
    }
    if (this.anyChange.consumers.size > 0) {
      refreshes.push(this.fireSubscribeAll());
    }
    await Promise.all(refreshes);
  }

  /** Re-issue every active wire subscribe after reconnect. The `.catch` suppresses
   *  unhandledRejection: the fire helpers re-throw so direct awaiters see failures, but
   *  replay-issued calls have no awaiter and the error already routed through reportError. */
  replayWireSubscriptions(): void {
    const swallow = (): void => undefined;
    for (const [propertyName, set] of this.properties) {
      if (set.consumers.size > 0) {
        set.wireSent = true;
        set.wireReady = this.firePropertySubscribe(propertyName);
        set.wireReady.catch(swallow);
      }
    }
    for (const [eventName, set] of this.events) {
      if (set.consumers.size > 0) {
        set.wireSent = true;
        set.wireReady = this.fireEventSubscribe(eventName);
        set.wireReady.catch(swallow);
      }
    }
    if (this.anyChange.consumers.size > 0) {
      this.anyChange.wireSent = true;
      this.fireSubscribeAll().catch(swallow);
    }
  }

  /** Forget which subscribes the server knows about, without sending unsubscribes. Post-disconnect. */
  invalidateWireState(): void {
    for (const set of this.properties.values()) set.wireSent = false;
    for (const set of this.events.values()) set.wireSent = false;
    this.anyChange.wireSent = false;
  }

  /** True iff at least one consumer is currently registered across any property, event,
   *  or anyChange subscription. Used by `InterestHandle.applyUpdate` to decide whether to
   *  drop a removed object's subscription entry: keep when consumers remain (sticky), drop
   *  when no one's listening (so the object never has to come back to release the entry). */
  hasAnySubscribers(): boolean {
    for (const set of this.properties.values()) if (set.consumers.size > 0) return true;
    for (const set of this.events.values()) if (set.consumers.size > 0) return true;
    return this.anyChange.consumers.size > 0;
  }

  // -- shared helpers ------------------------------------------------------------------------

  private subscribe<H>(
    set: ConsumerSet<H>,
    handler: H,
    opts: SubscribeOptions,
    onFirst: () => Promise<void>,
    onLast: () => Promise<void>,
  ): CancelFn {
    const consumer: Consumer<H> = { handler, cancelled: false };
    set.consumers.add(consumer);
    if (!set.wireSent) {
      set.wireSent = true;
      set.wireReady = onFirst();
      // Suppress unhandledRejection; the rejection is still observable by anyone awaiting
      // wireReady, and the underlying RPC also surfaces errors through reportError. The
      // rejection stays cached on the set until the last consumer cancels and tears the set
      // down; subsequent subscribes within the same set lifetime see the same failure rather
      // than retrying.
      set.wireReady.catch(() => {});
    }
    const cancel = (): void => {
      if (consumer.cancelled) return;
      consumer.cancelled = true;
      if (consumer.detachSignal) consumer.detachSignal();
      set.consumers.delete(consumer);
      if (set.consumers.size === 0 && set.wireSent) {
        set.wireSent = false;
        set.wireReady = undefined;
        void onLast();
      }
    };
    this.wireSignal(opts.signal, cancel, consumer);
    return cancel;
  }

  private fireSet<H>(set: ConsumerSet<H>, invoke: (handler: H) => void): void {
    for (const consumer of Array.from(set.consumers)) {
      if (consumer.cancelled) continue;
      try {
        invoke(consumer.handler);
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
  }

  private wireSignal<H>(
    signal: AbortSignal | undefined,
    cancel: CancelFn,
    consumer: Consumer<H>,
  ): void {
    if (!signal) return;
    if (signal.aborted) {
      cancel();
      return;
    }
    const onAbort = (): void => cancel();
    signal.addEventListener("abort", onAbort, { once: true });
    consumer.detachSignal = () => signal.removeEventListener("abort", onAbort);
  }

  // -- wire ops ------------------------------------------------------------------------------

  private async firePropertySubscribe(propertyName: string): Promise<void> {
    // Rate sent on every subscribe so the server's interval tracks the latest ask.
    const call: Parameters<JsonRpcClient["subscribeProperty"]>[0] = {
      interestName: this.interestName,
      objectName: this.objectName,
      propertyName,
    };
    if (this.maxRateHz !== null) call.maxRateHz = this.maxRateHz;
    try {
      await this.protocol.subscribeProperty(call);
    } catch (err) {
      // Surface via reportError for fire-and-forget callers, and re-throw so anyone awaiting
      // awaitPropertySubscribed sees the failure. Mirrors fireEventSubscribe.
      this.reportError(normalizeError(err));
      throw err;
    }
  }

  private async firePropertyUnsubscribe(propertyName: string): Promise<void> {
    try {
      await this.protocol.unsubscribeProperty({
        interestName: this.interestName,
        objectName: this.objectName,
        propertyName,
      });
    } catch (err) {
      this.reportError(normalizeError(err));
    }
  }

  private async fireEventSubscribe(eventName: string): Promise<void> {
    try {
      await this.protocol.subscribeEvent({
        interestName: this.interestName,
        objectName: this.objectName,
        eventName,
      });
    } catch (err) {
      // Surface via reportError for fire-and-forget callers, and re-throw so anyone awaiting
      // awaitEventSubscribed sees the failure.
      this.reportError(normalizeError(err));
      throw err;
    }
  }

  private async fireEventUnsubscribe(eventName: string): Promise<void> {
    try {
      await this.protocol.unsubscribeEvent({
        interestName: this.interestName,
        objectName: this.objectName,
        eventName,
      });
    } catch (err) {
      this.reportError(normalizeError(err));
    }
  }

  private async fireSubscribeAll(): Promise<void> {
    const call: Parameters<JsonRpcClient["subscribeAll"]>[0] = {
      interestName: this.interestName,
      objectName: this.objectName,
    };
    if (this.maxRateHz !== null) call.maxRateHz = this.maxRateHz;
    try {
      await this.protocol.subscribeAll(call);
    } catch (err) {
      this.reportError(normalizeError(err));
    }
  }

  private async fireUnsubscribeAll(): Promise<void> {
    try {
      await this.protocol.unsubscribeAll({
        interestName: this.interestName,
        objectName: this.objectName,
      });
    } catch (err) {
      this.reportError(normalizeError(err));
    }
  }
}
