// === handles.ts ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { ReportError } from "./connect.js";
import { InterestReleasedError, TransportError } from "./errors.js";
import {
  findMethodInClass,
  findPropertyInClass,
  getClassSpec,
} from "./internal/class_spec_lookup.js";
import { makeIdempotent } from "./internal/cancel.js";
import { encodeVar, encodeVarToWire, parseVar } from "./internal/var_codec.js";
import type { JsonRpcClient } from "./internal/protocol_client.js";
import { normalizeError } from "./internal/report_error.js";
import { ObjectSubscriptions } from "./internal/subscription_registry.js";
import type { TypeCache } from "./internal/type_cache.js";
import type {
  EventTriggeredNotification,
  InterestUpdateNotification,
  MaybeSubscribeBlock,
  PropertyChangedNotification,
} from "./generated/index.js";
import type { CancelFn, Var } from "./values.js";

/** Per-call overrides. `timeoutMs` overrides `ClientOptions.defaultTimeoutMs`. */
export interface CallOptions {
  timeoutMs?: number;
  signal?: AbortSignal;
}

/** Like {@link CallOptions}; `fresh: true` forces a server round-trip instead of returning a cached value. */
export interface GetOptions extends CallOptions {
  fresh?: boolean;
}

/** Receives a property's current value on subscribe and on every change. */
export type PropertyChangedHandler = (value: Var, info: DeliveryInfo) => void;

/**
 * Per-delivery metadata that came with a server push (event or property change). Same shape
 * for both because Sen routes property changes through its event mechanism on the kernel
 * side and surfaces only the timestamp.
 */
export interface DeliveryInfo {
  /** Server-side emission time. For events: kernel `EventInfo.creationTime`. For property
   *  changes: the object's `getLastCommitTime()` at bundling. Wire string format
   *  `"YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ"` (RFC-3339, UTC, nanosecond precision). Pass
   *  through {@link parseSenTimestamp} for a `Date` + nanosecond remainder. */
  timestamp: string;
}

/** Receives an event's positional arguments each time it fires. */
export type EventTriggeredHandler = (args: Var[], info: DeliveryInfo) => void;

/**
 * Parse a Sen wire timestamp (RFC-3339, UTC, nanosecond precision:
 * `"YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ"`) into a `Date` (ms precision) and a `nanoseconds`
 * remainder (the sub-millisecond portion, 0..999999). Returns `null` when the input
 * doesn't match the exact shape.
 */
export function parseSenTimestamp(s: string): { date: Date; nanoseconds: number } | null {
  const m = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})\.(\d{9})Z$/.exec(s.trim());
  if (!m) return null;
  const [, yr, mo, dy, hr, mi, sc, ns] = m;
  const nanos = parseInt(ns!, 10);
  const millis = Math.floor(nanos / 1_000_000);
  const remainder = nanos % 1_000_000;
  const date = new Date(
    Date.UTC(
      parseInt(yr!, 10),
      parseInt(mo!, 10) - 1,
      parseInt(dy!, 10),
      parseInt(hr!, 10),
      parseInt(mi!, 10),
      parseInt(sc!, 10),
      millis,
    ),
  );
  return { date, nanoseconds: remainder };
}

/** Receives one bundle of (property name -> value) per change tick on an object. */
export type AnyChangeHandler = (changes: Map<string, Var>, info: DeliveryInfo) => void;

function lookupPropertyTypeOn(className: string, propertyName: string, typeCache: TypeCache): string {
  const classSpec = getClassSpec(className, typeCache);
  const property = findPropertyInClass(classSpec, propertyName, typeCache);
  if (!property) {
    throw new TransportError(`property "${propertyName}" not found on class "${className}"`);
  }
  return property.type;
}

/** Per-object snapshot returned by {@link InterestHandle.getObjectsBatchState}. */
export interface ObjectBatchState {
  /** Object name within the interest's match set. */
  name: string;
  /** Package-qualified class name at read time. */
  className: string;
  /** Successfully read property values, keyed by property name. */
  properties: Record<string, Var>;
  /** Per-property read errors, keyed by property name. Empty when nothing failed. */
  errors: Record<string, string>;
}

/** Options for {@link InterestHandle.getObjectsBatchState}. */
export interface GetObjectsBatchStateOptions extends CallOptions {
  /** Subset of object names to read; omit to read every matched object. */
  objectNames?: string[];
  /** Subset of property names to read; omit to read every property of each object. */
  propertyNames?: string[];
}

/** Options for the `on*` subscribe-shaped APIs on {@link ObjectHandle}. */
export interface SubscribeOptions {
  /** AbortSignal alternative to the returned `CancelFn`. Cancels the subscription when fired. */
  signal?: AbortSignal;
}

/** Handle to one object in an interest's match set. */
export class ObjectHandle {
  /** @internal */
  constructor(
    public readonly name: string,
    public readonly className: string,
    private readonly interestName: string,
    private readonly protocol: JsonRpcClient,
    private readonly typeCache: TypeCache,
    private readonly subscriptions: ObjectSubscriptions,
  ) {}

  /**
   * Subscribe to a property's value changes. The handler fires whenever the server pushes a
   * value for the property; the first delivery typically arrives shortly after the wire
   * subscribe completes, and subsequent deliveries on every change. Returns a cancel function;
   * pass `{ signal }` for `AbortSignal` cancellation. To read the currently-cached value
   * without waiting, call `obj.get(name)`.
   *
   * @example
   * ```ts
   * const cancel = cat.onPropertyChanged("position", (pos) => console.log(pos));
   * // later: cancel();
   * ```
   */
  onPropertyChanged(
    name: string,
    handler: PropertyChangedHandler,
    opts: SubscribeOptions = {},
  ): CancelFn {
    return this.subscriptions.onPropertyChanged(name, handler, opts);
  }

  /**
   * Subscribe to an event. The handler receives the event's positional arguments.
   *
   * @example
   * ```ts
   * cat.onEventTriggered("madeSound", ([content]) => console.log("said:", content));
   * ```
   */
  onEventTriggered(
    name: string,
    handler: EventTriggeredHandler,
    opts: SubscribeOptions = {},
  ): CancelFn {
    return this.subscriptions.onEventTriggered(name, handler, opts);
  }

  /** Resolves once the server has acknowledged the active wire subscribe for `name`.
   *  Use immediately after onEventTriggered when you need to be sure the subscribe is in
   *  place before you trigger the event (e.g., subscribe-then-invokeMethod). No-op if no
   *  one is currently subscribed. */
  awaitEventSubscribed(name: string): Promise<void> {
    return this.subscriptions.awaitEventSubscribed(name);
  }

  /** Symmetric partner to {@link awaitEventSubscribed} for properties. Rejects with the
   *  server error when the property name is unknown or the subscribe call otherwise fails;
   *  call this when you need that signal beyond the `onError` side-channel. No-op if no one
   *  is currently subscribed. */
  awaitPropertySubscribed(name: string): Promise<void> {
    return this.subscriptions.awaitPropertySubscribed(name);
  }

  /** Subscribe to all of this object's property changes at once, delivered as one `Map` per update. */
  onAnyChange(handler: AnyChangeHandler, opts: SubscribeOptions = {}): CancelFn {
    return this.subscriptions.onAnyChange(handler, opts);
  }

  /** Cap how often this object's properties may push updates. Pass `null` to clear the cap. */
  async setMaxRateHz(hz: number | null): Promise<void> {
    await this.subscriptions.setMaxRateHz(hz);
  }

  /**
   * Read a property's current value. Returns from the local cache when an active subscription
   * is keeping it fresh; pass `{ fresh: true }` to force a server round-trip.
   *
   * @example
   * ```ts
   * const pos = await cat.get("position");
   * ```
   */
  async get(name: string, opts: GetOptions = {}): Promise<Var> {
    if (!opts.fresh) {
      const cached = this.subscriptions.getCachedValue(name);
      if (cached !== undefined) return cached;
    }
    const propType = this.lookupPropertyType(name);
    const encoded = await this.protocol.getProperty({
      interestName: this.interestName,
      objectName: this.name,
      propertyName: name,
      ...this.callOpts(opts),
    });
    const value = parseVar(propType, JSON.parse(encoded), this.typeCache);
    this.subscriptions.setCachedValue(name, value);
    return value;
  }

  /**
   * Write a property's value. Throws {@link JsonRpcError} if the property is not declared
   * `[writable]` in its STL.
   *
   * @example
   * ```ts
   * await shape.set("frozen", true);
   * ```
   */
  async set(name: string, value: unknown, opts: CallOptions = {}): Promise<void> {
    this.subscriptions.invalidateCachedValue(name);
    const propType = this.lookupPropertyType(name);
    const encoded = encodeVarToWire(propType, value, this.typeCache);
    await this.protocol.setProperty({
      interestName: this.interestName,
      objectName: this.name,
      propertyName: name,
      value: encoded,
      ...this.callOpts(opts),
    });
  }

  /**
   * Call a method with named arguments. Argument names are validated locally against the
   * method's declared signature before the call goes on the wire, so a typo throws immediately.
   * For optional parameters, pass `null` explicitly to mean "absent". Returns the method's
   * result, or `null` for void methods.
   *
   * @example
   * ```ts
   * await cat.invoke("jumpToLocation", { x: 42, y: 17 });
   * const reply = await person.invoke("ask", { question: "what time is it?" });
   * ```
   */
  async invoke(
    method: string,
    args: Record<string, unknown> = {},
    opts: CallOptions = {},
  ): Promise<Var> {
    const classSpec = getClassSpec(this.className, this.typeCache);
    const methodSpec = findMethodInClass(classSpec, method, this.typeCache);
    if (!methodSpec) {
      throw new TransportError(`method "${method}" not found on class "${this.className}"`);
    }
    const declaredNames = new Set(methodSpec.args.map((a) => a.name));
    for (const passedName of Object.keys(args)) {
      if (!declaredNames.has(passedName)) {
        throw new TransportError(
          `method "${method}" has no parameter "${passedName}"; expected: [${methodSpec.args.map((a) => a.name).join(", ")}]`,
        );
      }
    }
    const encodedArgs = methodSpec.args.map((arg) => {
      if (!(arg.name in args)) {
        throw new TransportError(`method "${method}" requires parameter "${arg.name}"`);
      }
      return encodeVar(arg.type, args[arg.name], this.typeCache);
    });
    // User-defined methods have no upper bound on legitimate runtime: a Fibonacci computation,
    // a long simulation step, a deferred external call. The client-side default of 2s here
    // would surface false TimeoutErrors mid-call, so invoke defaults to "no client timeout"
    // (transport.call honors timeoutMs <= 0 as "no timeout"). Connection loss still rejects
    // pending invocations via Transport.failAllPending. Callers can opt back in by passing an
    // explicit `timeoutMs` in CallOptions.
    const effectiveOpts: CallOptions = { timeoutMs: 0, ...opts };
    const encodedReturn = await this.protocol.invoke({
      interestName: this.interestName,
      objectName: this.name,
      methodName: method,
      argsJson: JSON.stringify(encodedArgs),
      ...this.callOpts(effectiveOpts),
    });
    if (methodSpec.returnType === "" || methodSpec.returnType === "void") return null;
    return parseVar(methodSpec.returnType, JSON.parse(encodedReturn), this.typeCache);
  }

  private lookupPropertyType(propertyName: string): string {
    return lookupPropertyTypeOn(this.className, propertyName, this.typeCache);
  }

  private callOpts(opts: CallOptions): { timeoutMs?: number; signal?: AbortSignal } {
    const out: { timeoutMs?: number; signal?: AbortSignal } = {};
    if (opts.timeoutMs !== undefined) out.timeoutMs = opts.timeoutMs;
    if (opts.signal !== undefined) out.signal = opts.signal;
    return out;
  }
}

export type ObjectAddedHandler = (obj: ObjectHandle) => void;
export type ObjectRemovedHandler = (name: string) => void;

/** @internal */
export type InterestReleasedCallback = (interestName: string) => void;

/**
 * Pre-subscription bundled with `Client.declareInterest`. When set, matched objects arrive
 * with their initial property values already populated, so the first `obj.get(name)` for a
 * subscribed property returns from cache without a wire round-trip.
 *
 * @example
 * ```ts
 * await client.declareInterest({
 *   name: "fleet",
 *   query: "SELECT aircraft.Aircraft FROM ops",
 *   subscribe: { properties: ["altitude", "speed"], maxRateHz: 10 },
 * });
 * ```
 */
export interface PreSubscription {
  /** `"*"` for every property, an array for specific names. Omit to skip property subscription. */
  properties?: "*" | string[];
  /** `"*"` for every event, an array for specific names. Omit to skip event subscription. */
  events?: "*" | string[];
  /** Cap on push rate from the server for this interest's subscribed properties. Omit for no throttle. */
  maxRateHz?: number;
}

/** @internal */
export interface InterestDeclaration {
  query: string;
  subscribe?: MaybeSubscribeBlock;
  withSchemas?: boolean;
}

/**
 * Lifecycle of an {@link InterestHandle}: `pending` (createInterest in flight), `active`
 * (server accepted; objects flowing), `releasing` (release in flight), `released` (terminal).
 */
export type InterestState = "pending" | "active" | "releasing" | "released";

/**
 * Handle to a declared interest: the live set of objects matching its query.
 *
 * After `release()` (or after the initial `declareInterest` was rejected), the synchronous
 * read accessors (`objects`, `objectByName`, `state`) keep working but return empty/`released`;
 * any method that would register a handler, start a wire call, or iterate (`onObjectAdded`,
 * `onObjectRemoved`, `getObjectsBatchState`, `for await (... of handle)`) throws
 * {@link InterestReleasedError}. Cleanup paths that race release can catch and ignore it.
 * `release()` itself stays idempotent.
 */
export class InterestHandle {
  private readonly objectsByName = new Map<string, ObjectHandle>();
  private readonly subscriptionsByObject = new Map<string, ObjectSubscriptions>();
  private readonly objectAddedHandlers = new Set<ObjectAddedHandler>();
  private readonly objectRemovedHandlers = new Set<ObjectRemovedHandler>();
  private readonly releaseWakers = new Set<() => void>();
  private cachedObjects: ObjectHandle[] | null = null;
  // Updates that arrive while the handle is still `pending` (server pushed the initial
  // match-set before the createInterest response landed). Drained by markActive in arrival
  // order; cleared by markRejected.
  private pendingUpdates: InterestUpdateNotification[] = [];

  private currentState: InterestState = "pending";
  private readonly activeReady: Promise<void>;
  private resolveActive!: () => void;
  private rejectActive!: (err: Error) => void;
  private readonly settledPromise: Promise<void>;
  private resolveSettled!: () => void;

  /** @internal */
  constructor(
    public readonly name: string,
    private readonly declaration: InterestDeclaration,
    private readonly protocol: JsonRpcClient,
    private readonly typeCache: TypeCache,
    private readonly onReleased: InterestReleasedCallback,
    private readonly reportError: ReportError,
  ) {
    this.activeReady = new Promise<void>((res, rej) => {
      this.resolveActive = res;
      this.rejectActive = rej;
    });
    // Absorb unhandled-rejection signal; real awaiters still see it via replay.
    this.activeReady.catch(() => undefined);
    this.settledPromise = new Promise<void>((res) => {
      this.resolveSettled = res;
    });
  }

  /** @internal Resolves when the handle reaches the terminal `released` state (whether via
   *  a successful release or a rejected initial declare). The `Client` uses this to chain the
   *  next declare for the same name. */
  get settled(): Promise<void> {
    return this.settledPromise;
  }

  get state(): InterestState {
    return this.currentState;
  }

  /**
   * Currently-matched objects. Synchronous; reads from the local cache kept in sync by the
   * server. The returned array reference is stable between membership changes; subsequent
   * calls return the same array until an `interestUpdate` adds, removes, or the handle is
   * released. Useful for consumers (React `useSyncExternalStore`, memoization, equality
   * checks) that rely on reference identity to detect change.
   *
   * Treat the returned array as immutable; mutations leak into the cache.
   */
  objects(): ObjectHandle[] {
    if (this.cachedObjects === null) {
      this.cachedObjects = Array.from(this.objectsByName.values());
    }
    return this.cachedObjects;
  }

  /** Look up a matched object by its Sen name, or `undefined` if no such object is currently in the set. */
  objectByName(name: string): ObjectHandle | undefined {
    return this.objectsByName.get(name);
  }

  /**
   * Read many objects' properties in a single round-trip. With no filter, returns one entry per
   * object currently in the match set, each carrying every property of its class hierarchy.
   * `objectNames` and `propertyNames` narrow the result. Property values are parsed via the
   * type cache; per-property failures (unknown name, accessor throw) land in `errors` so a
   * heterogeneous batch still returns useful partial results.
   *
   * Throws {@link InterestReleasedError} if the interest is `releasing`/`released`.
   */
  async getObjectsBatchState(opts: GetObjectsBatchStateOptions = {}): Promise<ObjectBatchState[]> {
    this.assertAlive("getObjectsBatchState");
    const wireOpts: Parameters<JsonRpcClient["getObjectsBatchState"]>[0] = {
      interestName: this.name,
      ...this.callOpts(opts),
    };
    if (opts.objectNames !== undefined) wireOpts.objectNames = opts.objectNames;
    if (opts.propertyNames !== undefined) wireOpts.propertyNames = opts.propertyNames;
    const states = await this.protocol.getObjectsBatchState(wireOpts);
    return states.map((state) => {
      const properties: Record<string, Var> = {};
      const errors: Record<string, string> = {};
      for (const { propertyName, error } of state.errors) {
        errors[propertyName] = error;
      }
      for (const { propertyName, value: encoded } of state.properties) {
        try {
          const propType = lookupPropertyTypeOn(state.qualifiedClassName, propertyName, this.typeCache);
          properties[propertyName] = parseVar(propType, JSON.parse(encoded), this.typeCache);
        } catch (err) {
          errors[propertyName] = err instanceof Error ? err.message : String(err);
        }
      }
      return { name: state.objectName, className: state.qualifiedClassName, properties, errors };
    });
  }

  private callOpts(opts: CallOptions): { timeoutMs?: number; signal?: AbortSignal } {
    const out: { timeoutMs?: number; signal?: AbortSignal } = {};
    if (opts.timeoutMs !== undefined) out.timeoutMs = opts.timeoutMs;
    if (opts.signal !== undefined) out.signal = opts.signal;
    return out;
  }

  /**
   * Notified when an object enters the match set. The handler fires immediately for every
   * object already matched and again for each later arrival. Mirrors the kernel-side
   * `ObjectList::onAdded` semantics, so the canonical pattern is to declare an interest,
   * register here, and let the handler drive the work.
   *
   * Throws {@link InterestReleasedError} if the interest is `releasing`/`released`.
   *
   * @example
   * ```ts
   * interest.onObjectAdded((obj) => {
   *   console.log("got", obj.className, obj.name);
   *   obj.onPropertyChanged("position", (p) => console.log(p));
   * });
   * ```
   */
  onObjectAdded(handler: ObjectAddedHandler): CancelFn {
    this.assertAlive("onObjectAdded");
    this.objectAddedHandlers.add(handler);
    // Replay current matches synchronously so a handler registered after declareInterest()
    // still sees objects that were in the initial server push.
    for (const obj of this.objectsByName.values()) {
      try {
        handler(obj);
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
    return makeIdempotent(() => {
      this.objectAddedHandlers.delete(handler);
    });
  }

  /**
   * Notified when an object leaves the match set. Throws {@link InterestReleasedError} if the
   * interest is `releasing`/`released`.
   */
  onObjectRemoved(handler: ObjectRemovedHandler): CancelFn {
    this.assertAlive("onObjectRemoved");
    this.objectRemovedHandlers.add(handler);
    return makeIdempotent(() => {
      this.objectRemovedHandlers.delete(handler);
    });
  }

  /** Release the interest. Idempotent. */
  async release(): Promise<void> {
    if (this.currentState === "released" || this.currentState === "releasing") return;
    if (this.currentState === "pending") {
      try {
        await this.activeReady;
      } catch {
        return;
      }
    }
    if (this.currentState !== "active") return;
    this.currentState = "releasing";
    this.objectsByName.clear();
    this.cachedObjects = null;
    this.subscriptionsByObject.clear();
    this.objectAddedHandlers.clear();
    this.objectRemovedHandlers.clear();
    this.wakeReleaseWaiters();
    this.onReleased(this.name);
    try {
      await this.protocol.releaseInterest({ interestName: this.name });
    } finally {
      this.currentState = "released";
      this.resolveSettled();
    }
  }

  /** @internal - Client calls this after a successful createInterest. */
  markActive(): void {
    if (this.currentState !== "pending") return;
    this.currentState = "active";
    this.resolveActive();
    if (this.pendingUpdates.length > 0) {
      const drained = this.pendingUpdates;
      this.pendingUpdates = [];
      for (const params of drained) this.applyUpdateActive(params);
    }
  }

  /** @internal - Client calls this after a failed createInterest. */
  markRejected(err: Error): void {
    if (this.currentState !== "pending") return;
    this.currentState = "released";
    this.pendingUpdates.length = 0;
    this.objectsByName.clear();
    this.cachedObjects = null;
    this.subscriptionsByObject.clear();
    this.objectAddedHandlers.clear();
    this.objectRemovedHandlers.clear();
    this.rejectActive(err);
    this.wakeReleaseWaiters();
    this.onReleased(this.name);
    this.resolveSettled();
  }

  /** @internal - Client routes interestUpdate notifications scoped to this.name here. */
  applyUpdate(params: InterestUpdateNotification): void {
    if (this.currentState === "releasing" || this.currentState === "released") return;
    if (this.currentState === "pending") {
      // Update arrived before createInterest's response; queue and drain on markActive so
      // the consumer's onObjectAdded handlers (registered after the declare resolves) see
      // the initial match-set. markRejected drops the queue.
      this.pendingUpdates.push(params);
      return;
    }
    this.applyUpdateActive(params);
  }

  private applyUpdateActive(params: InterestUpdateNotification): void {
    for (const removedName of params.removed) {
      if (this.objectsByName.delete(removedName)) {
        this.cachedObjects = null;
        this.fireRemoved(removedName);
      }
      // Subscriptions are sticky across remove/add; clear the value cache so a re-add
      // doesn't surface stale values. Drop the entry only if no consumer is still attached
      // (otherwise leaked across an object that never returns).
      const subs = this.subscriptionsByObject.get(removedName);
      if (subs) {
        subs.clearValueCache();
        if (!subs.hasAnySubscribers()) {
          this.subscriptionsByObject.delete(removedName);
        }
      }
    }
    for (const entry of params.added) {
      let subs = this.subscriptionsByObject.get(entry.objectName);
      if (subs && subs.qualifiedClassName !== entry.qualifiedClassName) {
        // Class changed under us; the cached ObjectSubscriptions would dispatch through
        // the wrong class spec. Drop it and start fresh.
        this.subscriptionsByObject.delete(entry.objectName);
        subs = undefined;
      }
      if (subs) {
        subs.invalidateWireState();
      } else {
        subs = new ObjectSubscriptions(
          this.protocol,
          this.typeCache,
          this.name,
          entry.objectName,
          entry.qualifiedClassName,
          this.reportError,
        );
        this.subscriptionsByObject.set(entry.objectName, subs);
      }
      const handle = new ObjectHandle(
        entry.objectName,
        entry.qualifiedClassName,
        this.name,
        this.protocol,
        this.typeCache,
        subs,
      );
      this.objectsByName.set(entry.objectName, handle);
      this.cachedObjects = null;

      subs.seedCurrentValues(entry.currentValues ?? []);
      subs.replayWireSubscriptions();
      this.fireAdded(handle);
    }
  }

  /** @internal */
  dispatchPropertyChanged(params: PropertyChangedNotification): void {
    if (this.currentState === "releasing" || this.currentState === "released") return;
    this.subscriptionsByObject.get(params.objectName)?.dispatchPropertyChanged(params);
  }

  /** @internal */
  dispatchEventTriggered(params: EventTriggeredNotification): void {
    if (this.currentState === "releasing" || this.currentState === "released") return;
    this.subscriptionsByObject
      .get(params.objectName)
      ?.dispatchEventTriggered(params.eventName, params.args, { timestamp: params.timestamp });
  }

  /** Connection epoch this interest last (re-)declared on. Makes `reestablish` idempotent
   *  per connection generation: the second trigger of the same recovery (autoReestablish
   *  plus a manual `reestablishAll()`, sequential or concurrent) must not clear and
   *  re-declare an interest that is already live on the current connection - the re-create
   *  bounces off "interest already exists" and the churn double-fires removed/added. */
  private lastReestablishedEpoch = Number.NEGATIVE_INFINITY;

  /** @internal - called by Client when the interest is (re-)declared on `epoch`. */
  noteReestablished(epoch: number): void {
    this.lastReestablishedEpoch = epoch;
  }

  /** @internal - called by Client.reestablishAll. */
  async reestablish(epoch: number): Promise<void> {
    if (this.currentState !== "active") return;
    if (epoch === this.lastReestablishedEpoch) return;

    for (const name of Array.from(this.objectsByName.keys())) {
      this.objectsByName.delete(name);
      this.cachedObjects = null;
      this.fireRemoved(name);
      this.subscriptionsByObject.get(name)?.clearValueCache();
    }
    for (const subs of this.subscriptionsByObject.values()) {
      subs.invalidateWireState();
    }
    const call: Parameters<JsonRpcClient["createInterest"]>[0] = {
      interestName: this.name,
      query: this.declaration.query,
    };
    if (this.declaration.subscribe !== undefined) call.subscribe = this.declaration.subscribe;
    if (this.declaration.withSchemas === true) call.withSchemas = true;
    try {
      await this.protocol.createInterest(call);
    } catch (err) {
      await this.resyncAfterFailedReestablish(err);
    }
    this.noteReestablished(epoch);
  }

  /** A rejected re-declare does not have to mean the interest is gone: a racing sibling
   *  reestablish may already have re-created it server-side ("interest already exists"),
   *  in which case its interestUpdate landed before our clear above and was wiped. Probe
   *  `listObjects`: if the interest exists, rebuild the match set from the authoritative
   *  answer (type specs are already cached - whichever call created the interest shipped
   *  them on this connection); if it doesn't, surface the original error. */
  private async resyncAfterFailedReestablish(originalError: unknown): Promise<void> {
    let infos: Awaited<ReturnType<JsonRpcClient["listObjects"]>>;
    try {
      infos = await this.protocol.listObjects({ interestName: this.name });
    } catch {
      throw originalError instanceof Error ? originalError : new Error(String(originalError));
    }
    this.applyUpdateActive({
      interestName: this.name,
      added: infos.map((info) => ({
        objectName: info.objectName,
        qualifiedClassName: info.qualifiedClassName,
        currentValues: [],
      })),
      removed: [],
      types: [],
      typeSchemas: "",
    });
  }

  private fireAdded(obj: ObjectHandle): void {
    for (const handler of Array.from(this.objectAddedHandlers)) {
      try {
        handler(obj);
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
  }

  private fireRemoved(objectName: string): void {
    for (const handler of Array.from(this.objectRemovedHandlers)) {
      try {
        handler(objectName);
      } catch (err) {
        this.reportError(normalizeError(err));
      }
    }
  }

  private wakeReleaseWaiters(): void {
    const wakers = Array.from(this.releaseWakers);
    this.releaseWakers.clear();
    for (const wake of wakers) wake();
  }

  /**
   * Async-iterate the match set: yields every currently-matched object, then each later
   * arrival, until the interest is released or the loop is broken. Thin wrapper over
   * {@link onObjectAdded} for ergonomic `for await` use.
   *
   * Throws {@link InterestReleasedError} if the interest is already `releasing`/`released`
   * when the iteration starts. If the interest is released mid-iteration, the loop ends
   * cleanly (no throw).
   *
   * @example
   * ```ts
   * for await (const cat of interest) {
   *   console.log(cat.name);
   *   if (cat.name === "rufus") break;  // break to stop iterating
   * }
   * ```
   */
  async *[Symbol.asyncIterator](): AsyncIterableIterator<ObjectHandle> {
    this.assertAlive("asyncIterator");
    const queue: ObjectHandle[] = [];
    let waker: (() => void) | null = null;
    const wake = (): void => {
      const w = waker;
      waker = null;
      w?.();
    };

    const cancelAdded = this.onObjectAdded((obj) => {
      queue.push(obj);
      wake();
    });
    this.releaseWakers.add(wake);

    try {
      while (true) {
        while (queue.length > 0) {
          yield queue.shift()!;
        }
        if (!this.isAlive()) return;
        await new Promise<void>((r) => {
          waker = r;
        });
      }
    } finally {
      cancelAdded();
      this.releaseWakers.delete(wake);
    }
  }

  private isAlive(): boolean {
    return this.currentState !== "releasing" && this.currentState !== "released";
  }

  private assertAlive(operation: string): void {
    if (!this.isAlive()) throw new InterestReleasedError(this.name, operation);
  }
}
