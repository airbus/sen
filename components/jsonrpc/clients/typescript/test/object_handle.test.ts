// === object_handle.test.ts ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";
import { ObjectHandle } from "../src/handles.js";
import { JsonRpcClient, type TransportLike } from "../src/internal/protocol_client.js";
import { ObjectSubscriptions } from "../src/internal/subscription_registry.js";
import { TypeCache } from "../src/internal/type_cache.js";
import { TransportError } from "../src/index.js";
import type { CancelFn, CustomTypeSpec } from "../src/index.js";

const aircraftClassSpec: CustomTypeSpec = {
  name: "Aircraft",
  qualifiedName: "demo.Aircraft",
  description: "",
  data: {
    type: "sen.kernel.ClassTypeSpec",
    value: {
      properties: [
        {
          name: "altitude",
          description: "",
          category: "dynamicRO",
          type: "f64",
          transportMode: "unicast",
          tags: [],
          checkedSet: false,
        },
        {
          name: "label",
          description: "",
          category: "dynamicRW",
          type: "string",
          transportMode: "unicast",
          tags: [],
          checkedSet: false,
        },
      ],
      methods: [
        {
          name: "land",
          description: "",
          args: [{ name: "speed", description: "", type: "f64" }],
          transportMode: "unicast",
          constness: "nonConstant",
          deferred: false,
          returnType: "bool",
          propertyRelation: "nonPropertyRelated",
          localOnly: false,
        },
        {
          name: "noop",
          description: "",
          args: [],
          transportMode: "unicast",
          constness: "nonConstant",
          deferred: false,
          returnType: "",
          propertyRelation: "nonPropertyRelated",
          localOnly: false,
        },
      ],
      events: [],
      constructor: {
        name: "Aircraft",
        description: "",
        args: [],
        transportMode: "unicast",
        constness: "nonConstant",
        deferred: false,
        returnType: "",
        propertyRelation: "nonPropertyRelated",
        localOnly: false,
      },
      parents: [],
      isInterface: false,
    },
  },
};

function makeRecordingTransport(responses: Map<string, unknown> = new Map()): {
  transport: TransportLike;
  call: ReturnType<typeof vi.fn>;
} {
  const call = vi.fn(async (req: { method: string }) => {
    return responses.get(req.method);
  });
  const onNotification = vi.fn((): CancelFn => () => undefined);
  return { transport: { call, onNotification }, call };
}

function makeHandle(transport: TransportLike, cache?: TypeCache): ObjectHandle {
  const typeCache = cache ?? new TypeCache();
  if (!typeCache.has("demo.Aircraft")) typeCache.set(aircraftClassSpec);
  const reportError = (): void => undefined;
  const protocol = new JsonRpcClient(transport, reportError);
  const subs = new ObjectSubscriptions(
    protocol,
    typeCache,
    "i1",
    "a1",
    "demo.Aircraft",
    reportError,
  );
  return new ObjectHandle("a1", "demo.Aircraft", "i1", protocol, typeCache, subs);
}

describe("ObjectHandle.get", () => {
  it("looks up the property type via the class spec and parses the response", async () => {
    const { transport, call } = makeRecordingTransport(new Map([["getProperty", "1234.5"]]));
    const handle = makeHandle(transport);
    await expect(handle.get("altitude")).resolves.toBe(1234.5);
    expect(call).toHaveBeenCalledWith(
      expect.objectContaining({
        method: "getProperty",
        params: { interestName: "i1", objectName: "a1", propertyName: "altitude" },
      }),
    );
  });

  it("throws TransportError for an unknown property name", async () => {
    const { transport } = makeRecordingTransport();
    const handle = makeHandle(transport);
    await expect(handle.get("unknown")).rejects.toThrow(/property "unknown" not found/);
  });
});

describe("ObjectHandle.set", () => {
  it("encodes the value against the property type and sends it as a string", async () => {
    const { transport, call } = makeRecordingTransport();
    const handle = makeHandle(transport);
    await handle.set("label", "hello");
    expect(call).toHaveBeenCalledWith(
      expect.objectContaining({
        method: "setProperty",
        params: { interestName: "i1", objectName: "a1", propertyName: "label", value: '"hello"' },
      }),
    );
  });

  it("rejects a value whose shape doesn't match the property type", async () => {
    const { transport } = makeRecordingTransport();
    const handle = makeHandle(transport);
    await expect(handle.set("altitude", "not-a-number")).rejects.toThrow(TransportError);
  });
});

describe("ObjectHandle.invoke", () => {
  it("encodes named args by spec order, sends argsJson, and parses the return", async () => {
    const { transport, call } = makeRecordingTransport(new Map([["invoke", "true"]]));
    const handle = makeHandle(transport);
    await expect(handle.invoke("land", { speed: 120 })).resolves.toBe(true);
    expect(call).toHaveBeenCalledWith(
      expect.objectContaining({
        method: "invoke",
        params: {
          interestName: "i1",
          objectName: "a1",
          methodName: "land",
          argsJson: "[120]",
        },
      }),
    );
  });

  it("returns null for void methods regardless of the wire payload", async () => {
    const { transport } = makeRecordingTransport(new Map([["invoke", "null"]]));
    const handle = makeHandle(transport);
    await expect(handle.invoke("noop")).resolves.toBeNull();
  });

  it("rejects when a required arg is omitted", async () => {
    const { transport } = makeRecordingTransport();
    const handle = makeHandle(transport);
    await expect(handle.invoke("land", {})).rejects.toThrow(
      /method "land" requires parameter "speed"/,
    );
  });

  it("rejects when an unknown arg name is passed", async () => {
    const { transport } = makeRecordingTransport();
    const handle = makeHandle(transport);
    await expect(handle.invoke("land", { speedd: 120 })).rejects.toThrow(
      /method "land" has no parameter "speedd"; expected: \[speed\]/,
    );
  });

  it("throws TransportError for an unknown method name", async () => {
    const { transport } = makeRecordingTransport();
    const handle = makeHandle(transport);
    await expect(handle.invoke("teleport")).rejects.toThrow(/method "teleport" not found/);
  });
});
