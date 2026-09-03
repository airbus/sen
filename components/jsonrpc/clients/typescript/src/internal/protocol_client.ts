// === protocol_client.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { TransportError } from "../errors.js";
import {
  isEventTriggeredNotification,
  isInterestUpdateNotification,
  isNotificationsDroppedNotification,
  isPropertyChangedNotification,
  isTopologyChangedNotification,
} from "./notification_validators.js";
import type {
  EventTriggeredNotification,
  InterestUpdateNotification,
  MaybeFlag,
  MaybeRateHz,
  MaybeSubscribeBlock,
  ObjectInfos,
  ObjectStateList,
  NotificationsDroppedNotification,
  PropertyChangedNotification,
  SessionInfoList,
  StringList,
  TopologyChangedNotification,
  TypeLookupResult,
} from "../generated/index.js";
import type { ReportError } from "../connect.js";
import type { CancelFn } from "../values.js";

/** Subset of `Transport` that JsonRpcClient needs; lets tests inject a fake. */
export interface TransportLike {
  call(request: {
    method: string;
    params?: unknown;
    timeoutMs?: number;
    signal?: AbortSignal;
  }): Promise<unknown>;
  onNotification(args: {
    method: string;
    handler: (params: unknown) => void;
  }): CancelFn;
}

export interface CallOptions {
  timeoutMs?: number;
  signal?: AbortSignal;
}

// Omits absent optional keys; required for exactOptionalPropertyTypes.
function buildRequest(
  method: string,
  wireParams: unknown,
  callOpts: CallOptions,
): {
  method: string;
  params?: unknown;
  timeoutMs?: number;
  signal?: AbortSignal;
} {
  const req: { method: string; params?: unknown; timeoutMs?: number; signal?: AbortSignal } = {
    method,
  };
  if (wireParams !== undefined) req.params = wireParams;
  if (callOpts.timeoutMs !== undefined) req.timeoutMs = callOpts.timeoutMs;
  if (callOpts.signal !== undefined) req.signal = callOpts.signal;
  return req;
}

/** One method per RPC in `JsonRpcServer.stl`; wire-faithful naming. */
export class JsonRpcClient {
  constructor(
    private readonly transport: TransportLike,
    readonly reportError: ReportError = (err) => {
      console.error(err);
    },
  ) {}

  // -- health + introspection ----------------------------------------------------------------

  async ping(opts: CallOptions = {}): Promise<string> {
    return (await this.transport.call(buildRequest("ping", undefined, opts))) as string;
  }

  // -- topology (sessions + buses) -----------------------------------------------------------

  async listTopology(opts: CallOptions = {}): Promise<SessionInfoList> {
    return (await this.transport.call(buildRequest("listTopology", undefined, opts))) as SessionInfoList;
  }

  async subscribeTopology(opts: CallOptions = {}): Promise<void> {
    await this.transport.call(buildRequest("subscribeTopology", undefined, opts));
  }

  async unsubscribeTopology(opts: CallOptions = {}): Promise<void> {
    await this.transport.call(buildRequest("unsubscribeTopology", undefined, opts));
  }

  async getTypes(opts: CallOptions = {}): Promise<StringList> {
    return (await this.transport.call(buildRequest("getTypes", undefined, opts))) as StringList;
  }

  async getType(
    params: { qualifiedName: string; withSchema?: MaybeFlag } & CallOptions,
  ): Promise<TypeLookupResult> {
    return (await this.transport.call(
      buildRequest(
        "getType",
        {
          qualifiedName: params.qualifiedName,
          withSchema: params.withSchema ?? null,
        },
        params,
      ),
    )) as TypeLookupResult;
  }

  // -- interests -----------------------------------------------------------------------------

  async createInterest(
    params: {
      interestName: string;
      query: string;
      subscribe?: MaybeSubscribeBlock;
      withSchemas?: MaybeFlag;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "createInterest",
        {
          interestName: params.interestName,
          query: params.query,
          subscribe: params.subscribe ?? null,
          withSchemas: params.withSchemas ?? null,
        },
        params,
      ),
    );
  }

  async releaseInterest(params: { interestName: string } & CallOptions): Promise<void> {
    await this.transport.call(
      buildRequest("releaseInterest", { interestName: params.interestName }, params),
    );
  }

  async listObjects(params: { interestName: string } & CallOptions): Promise<ObjectInfos> {
    return (await this.transport.call(
      buildRequest("listObjects", { interestName: params.interestName }, params),
    )) as ObjectInfos;
  }

  // -- read/write (Var is string-encoded on the wire) ---------------------------------------

  async getProperty(
    params: {
      interestName: string;
      objectName: string;
      propertyName: string;
    } & CallOptions,
  ): Promise<string> {
    return (await this.transport.call(
      buildRequest(
        "getProperty",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          propertyName: params.propertyName,
        },
        params,
      ),
    )) as string;
  }

  async getObjectsBatchState(
    params: {
      interestName: string;
      /** Subset of object names to read; omit to read every matched object. */
      objectNames?: StringList;
      /** Subset of property names to read; omit to read every property of each object. */
      propertyNames?: StringList;
    } & CallOptions,
  ): Promise<ObjectStateList> {
    return (await this.transport.call(
      buildRequest(
        "getObjectsBatchState",
        {
          interestName: params.interestName,
          objectNames: params.objectNames ?? null,
          propertyNames: params.propertyNames ?? null,
        },
        params,
      ),
    )) as ObjectStateList;
  }

  async setProperty(
    params: {
      interestName: string;
      objectName: string;
      propertyName: string;
      /** JSON-encoded Var as string. */
      value: string;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "setProperty",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          propertyName: params.propertyName,
          value: params.value,
        },
        params,
      ),
    );
  }

  // -- sticky subscriptions ------------------------------------------------------------------

  async subscribeProperty(
    params: {
      interestName: string;
      objectName: string;
      propertyName: string;
      maxRateHz?: MaybeRateHz;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "subscribeProperty",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          propertyName: params.propertyName,
          maxRateHz: params.maxRateHz ?? null,
        },
        params,
      ),
    );
  }

  async unsubscribeProperty(
    params: {
      interestName: string;
      objectName: string;
      propertyName: string;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "unsubscribeProperty",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          propertyName: params.propertyName,
        },
        params,
      ),
    );
  }

  async subscribeEvent(
    params: {
      interestName: string;
      objectName: string;
      eventName: string;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "subscribeEvent",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          eventName: params.eventName,
        },
        params,
      ),
    );
  }

  async unsubscribeEvent(
    params: {
      interestName: string;
      objectName: string;
      eventName: string;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "unsubscribeEvent",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          eventName: params.eventName,
        },
        params,
      ),
    );
  }

  async subscribeAll(
    params: {
      interestName: string;
      objectName: string;
      maxRateHz?: MaybeRateHz;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "subscribeAll",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          maxRateHz: params.maxRateHz ?? null,
        },
        params,
      ),
    );
  }

  async unsubscribeAll(
    params: {
      interestName: string;
      objectName: string;
    } & CallOptions,
  ): Promise<void> {
    await this.transport.call(
      buildRequest(
        "unsubscribeAll",
        {
          interestName: params.interestName,
          objectName: params.objectName,
        },
        params,
      ),
    );
  }

  // -- meta-invoke ---------------------------------------------------------------------------

  async invoke(
    params: {
      interestName: string;
      objectName: string;
      methodName: string;
      /** JSON array as string (positional method arguments). */
      argsJson: string;
    } & CallOptions,
  ): Promise<string> {
    return (await this.transport.call(
      buildRequest(
        "invoke",
        {
          interestName: params.interestName,
          objectName: params.objectName,
          methodName: params.methodName,
          argsJson: params.argsJson,
        },
        params,
      ),
    )) as string;
  }

  // -- notification dispatch -----------------------------------------------------------------
  // Each helper validates the raw payload against a typed predicate; mismatches route through
  // reportError so a malformed frame doesn't crash the dispatch loop.

  onPropertyChanged(handler: (params: PropertyChangedNotification) => void): CancelFn {
    return this.transport.onNotification({
      method: "propertyChanged",
      handler: (raw) => {
        if (!isPropertyChangedNotification(raw)) {
          this.reportError(
            new TransportError(`malformed propertyChanged payload: ${JSON.stringify(raw)}`),
          );
          return;
        }
        handler(raw);
      },
    });
  }

  onEventTriggered(handler: (params: EventTriggeredNotification) => void): CancelFn {
    return this.transport.onNotification({
      method: "eventTriggered",
      handler: (raw) => {
        if (!isEventTriggeredNotification(raw)) {
          this.reportError(
            new TransportError(`malformed eventTriggered payload: ${JSON.stringify(raw)}`),
          );
          return;
        }
        handler(raw);
      },
    });
  }

  onInterestUpdate(handler: (params: InterestUpdateNotification) => void): CancelFn {
    return this.transport.onNotification({
      method: "interestUpdate",
      handler: (raw) => {
        if (!isInterestUpdateNotification(raw)) {
          this.reportError(
            new TransportError(`malformed interestUpdate payload: ${JSON.stringify(raw)}`),
          );
          return;
        }
        handler(raw);
      },
    });
  }

  onTopologyChanged(handler: (params: TopologyChangedNotification) => void): CancelFn {
    return this.transport.onNotification({
      method: "topologyChanged",
      handler: (raw) => {
        if (!isTopologyChangedNotification(raw)) {
          this.reportError(
            new TransportError(`malformed topologyChanged payload: ${JSON.stringify(raw)}`),
          );
          return;
        }
        handler(raw);
      },
    });
  }

  onNotificationsDropped(handler: (params: NotificationsDroppedNotification) => void): CancelFn {
    return this.transport.onNotification({
      method: "notificationsDropped",
      handler: (raw) => {
        if (!isNotificationsDroppedNotification(raw)) {
          this.reportError(
            new TransportError(`malformed notificationsDropped payload: ${JSON.stringify(raw)}`),
          );
          return;
        }
        handler(raw);
      },
    });
  }
}
