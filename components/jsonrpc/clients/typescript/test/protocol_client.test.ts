// === protocol_client.test.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";
import { JsonRpcClient, type TransportLike } from "../src/internal/protocol_client.js";
import type {
  CancelFn,
  CustomTypeSpec,
  EventTriggeredNotification,
  InterestUpdateNotification,
  ObjectInfos,
  PropertyChangedNotification,
} from "../src/index.js";
import type { SubscribeBlock } from "../src/generated/index.js";

/** Minimal recording transport for unit-testing the Layer 2 wrappers. */
function makeRecordingTransport(): {
  transport: TransportLike;
  call: ReturnType<typeof vi.fn>;
  onNotification: ReturnType<typeof vi.fn>;
  setResponse(value: unknown): void;
} {
  let nextResponse: unknown;
  const call = vi.fn(async (_req: { method: string; params?: unknown }) => nextResponse);
  const onNotification = vi.fn(
    (_args: { method: string; handler: (params: unknown) => void }): CancelFn => () => undefined,
  );
  return {
    transport: { call, onNotification },
    call,
    onNotification,
    setResponse(value) {
      nextResponse = value;
    },
  };
}

describe("JsonRpcClient -- health + introspection", () => {
  it("ping sends the right method and surfaces the result", async () => {
    const t = makeRecordingTransport();
    t.setResponse("pong");
    const client = new JsonRpcClient(t.transport);
    await expect(client.ping()).resolves.toBe("pong");
    expect(t.call).toHaveBeenCalledWith({ method: "ping" });
  });

  it("listTopology returns the SessionInfoList result", async () => {
    const t = makeRecordingTransport();
    t.setResponse([{ name: "s1", buses: ["b1"] }]);
    const client = new JsonRpcClient(t.transport);
    await expect(client.listTopology()).resolves.toEqual([{ name: "s1", buses: ["b1"] }]);
    expect(t.call).toHaveBeenCalledWith({ method: "listTopology" });
  });

  it("subscribeTopology sends the bare method", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.subscribeTopology();
    expect(t.call).toHaveBeenCalledWith({ method: "subscribeTopology" });
  });

  it("unsubscribeTopology sends the bare method", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.unsubscribeTopology();
    expect(t.call).toHaveBeenCalledWith({ method: "unsubscribeTopology" });
  });

  it("getTypes returns the StringList result", async () => {
    const t = makeRecordingTransport();
    t.setResponse(["A", "B"]);
    const client = new JsonRpcClient(t.transport);
    await expect(client.getTypes()).resolves.toEqual(["A", "B"]);
    expect(t.call).toHaveBeenCalledWith({ method: "getTypes" });
  });

  it("getType sends qualifiedName + withSchema:null by default and returns TypeLookupResult", async () => {
    const t = makeRecordingTransport();
    const spec: CustomTypeSpec = {
      name: "X",
      qualifiedName: "pkg.X",
      description: "",
      data: { type: "sen.kernel.AliasTypeSpec", value: { aliasedType: "string" } },
    };
    const result = { spec, schema: "" };
    t.setResponse(result);
    const client = new JsonRpcClient(t.transport);
    await expect(client.getType({ qualifiedName: "pkg.X" })).resolves.toEqual(result);
    expect(t.call).toHaveBeenCalledWith({
      method: "getType",
      params: { qualifiedName: "pkg.X", withSchema: null },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("getType forwards withSchema:true so the wire ships the schema fragment", async () => {
    const t = makeRecordingTransport();
    const result = {
      spec: {
        name: "X",
        qualifiedName: "pkg.X",
        description: "",
        data: { type: "sen.kernel.AliasTypeSpec", value: { aliasedType: "string" } },
      },
      schema: '{"$id":"pkg.X","type":"string"}',
    };
    t.setResponse(result);
    const client = new JsonRpcClient(t.transport);
    await expect(client.getType({ qualifiedName: "pkg.X", withSchema: true })).resolves.toEqual(result);
    expect(t.call).toHaveBeenCalledWith({
      method: "getType",
      params: { qualifiedName: "pkg.X", withSchema: true },
      timeoutMs: undefined,
      signal: undefined,
    });
  });
});

describe("JsonRpcClient -- interests", () => {
  it("createInterest sends wire-faithful params and defaults subscribe + withSchemas to null", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.createInterest({ interestName: "i1", query: "SELECT * FROM x" });
    expect(t.call).toHaveBeenCalledWith({
      method: "createInterest",
      params: { interestName: "i1", query: "SELECT * FROM x", subscribe: null, withSchemas: null },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("createInterest forwards a provided subscribe block", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    const sub: SubscribeBlock = {
      properties: { type: "sen.components.jsonrpc.WildcardSelection", value: {} },
      events: null,
      maxRateHz: 30,
    };
    await client.createInterest({ interestName: "i1", query: "q", subscribe: sub });
    expect(t.call).toHaveBeenCalledWith({
      method: "createInterest",
      params: { interestName: "i1", query: "q", subscribe: sub, withSchemas: null },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("createInterest forwards withSchemas:true so the wire opts into schema shipping", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.createInterest({ interestName: "i1", query: "q", withSchemas: true });
    expect(t.call).toHaveBeenCalledWith({
      method: "createInterest",
      params: { interestName: "i1", query: "q", subscribe: null, withSchemas: true },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("releaseInterest sends interestName", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.releaseInterest({ interestName: "i1" });
    expect(t.call).toHaveBeenCalledWith({
      method: "releaseInterest",
      params: { interestName: "i1" },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("listObjects returns the ObjectInfos result", async () => {
    const t = makeRecordingTransport();
    const list: ObjectInfos = [
      { objectName: "a", qualifiedClassName: "pkg.A" },
      { objectName: "b", qualifiedClassName: "pkg.B" },
    ];
    t.setResponse(list);
    const client = new JsonRpcClient(t.transport);
    await expect(client.listObjects({ interestName: "i1" })).resolves.toEqual(list);
    expect(t.call).toHaveBeenCalledWith({
      method: "listObjects",
      params: { interestName: "i1" },
      timeoutMs: undefined,
      signal: undefined,
    });
  });
});

describe("JsonRpcClient -- read/write", () => {
  it("getProperty returns the string result verbatim (string-contract)", async () => {
    const t = makeRecordingTransport();
    t.setResponse("12345.6");
    const client = new JsonRpcClient(t.transport);
    await expect(
      client.getProperty({ interestName: "i", objectName: "o", propertyName: "p" }),
    ).resolves.toBe("12345.6");
    expect(t.call).toHaveBeenCalledWith({
      method: "getProperty",
      params: { interestName: "i", objectName: "o", propertyName: "p" },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("setProperty sends value as a string verbatim", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.setProperty({
      interestName: "i",
      objectName: "o",
      propertyName: "p",
      value: '"hello"',
    });
    expect(t.call).toHaveBeenCalledWith({
      method: "setProperty",
      params: { interestName: "i", objectName: "o", propertyName: "p", value: '"hello"' },
      timeoutMs: undefined,
      signal: undefined,
    });
  });
});

describe("JsonRpcClient -- sticky subscriptions", () => {
  it("subscribeProperty defaults maxRateHz to null when omitted", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.subscribeProperty({ interestName: "i", objectName: "o", propertyName: "p" });
    expect(t.call).toHaveBeenCalledWith({
      method: "subscribeProperty",
      params: { interestName: "i", objectName: "o", propertyName: "p", maxRateHz: null },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("subscribeProperty forwards a provided maxRateHz", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.subscribeProperty({
      interestName: "i",
      objectName: "o",
      propertyName: "p",
      maxRateHz: 60,
    });
    expect(t.call).toHaveBeenCalledWith({
      method: "subscribeProperty",
      params: { interestName: "i", objectName: "o", propertyName: "p", maxRateHz: 60 },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("unsubscribeProperty sends the wire-faithful params", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.unsubscribeProperty({ interestName: "i", objectName: "o", propertyName: "p" });
    expect(t.call).toHaveBeenCalledWith({
      method: "unsubscribeProperty",
      params: { interestName: "i", objectName: "o", propertyName: "p" },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("subscribeEvent + unsubscribeEvent send wire-faithful params", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.subscribeEvent({ interestName: "i", objectName: "o", eventName: "e" });
    expect(t.call).toHaveBeenLastCalledWith({
      method: "subscribeEvent",
      params: { interestName: "i", objectName: "o", eventName: "e" },
      timeoutMs: undefined,
      signal: undefined,
    });
    await client.unsubscribeEvent({ interestName: "i", objectName: "o", eventName: "e" });
    expect(t.call).toHaveBeenLastCalledWith({
      method: "unsubscribeEvent",
      params: { interestName: "i", objectName: "o", eventName: "e" },
      timeoutMs: undefined,
      signal: undefined,
    });
  });

  it("subscribeAll + unsubscribeAll send wire-faithful params", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    await client.subscribeAll({ interestName: "i", objectName: "o" });
    expect(t.call).toHaveBeenLastCalledWith({
      method: "subscribeAll",
      params: { interestName: "i", objectName: "o", maxRateHz: null },
      timeoutMs: undefined,
      signal: undefined,
    });
    await client.subscribeAll({ interestName: "i", objectName: "o", maxRateHz: 10 });
    expect(t.call).toHaveBeenLastCalledWith({
      method: "subscribeAll",
      params: { interestName: "i", objectName: "o", maxRateHz: 10 },
      timeoutMs: undefined,
      signal: undefined,
    });
    await client.unsubscribeAll({ interestName: "i", objectName: "o" });
    expect(t.call).toHaveBeenLastCalledWith({
      method: "unsubscribeAll",
      params: { interestName: "i", objectName: "o" },
      timeoutMs: undefined,
      signal: undefined,
    });
  });
});

describe("JsonRpcClient -- invoke", () => {
  it("invoke sends argsJson verbatim and returns the string result", async () => {
    const t = makeRecordingTransport();
    t.setResponse('"result-as-encoded-string"');
    const client = new JsonRpcClient(t.transport);
    const result = await client.invoke({
      interestName: "i",
      objectName: "o",
      methodName: "doSomething",
      argsJson: "[1, 2, 3]",
    });
    expect(result).toBe('"result-as-encoded-string"');
    expect(t.call).toHaveBeenCalledWith({
      method: "invoke",
      params: {
        interestName: "i",
        objectName: "o",
        methodName: "doSomething",
        argsJson: "[1, 2, 3]",
      },
      timeoutMs: undefined,
      signal: undefined,
    });
  });
});

describe("JsonRpcClient -- per-call options pass-through", () => {
  it("timeoutMs and signal flow into transport.call", async () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    const ctrl = new AbortController();
    await client.releaseInterest({ interestName: "i", timeoutMs: 5000, signal: ctrl.signal });
    expect(t.call).toHaveBeenCalledWith({
      method: "releaseInterest",
      params: { interestName: "i" },
      timeoutMs: 5000,
      signal: ctrl.signal,
    });
  });
});

describe("JsonRpcClient -- notification dispatch", () => {
  it("onPropertyChanged registers a handler under 'propertyChanged' and casts the payload", () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    const userHandler = vi.fn();
    const cancel = client.onPropertyChanged(userHandler);
    expect(t.onNotification).toHaveBeenCalledWith({
      method: "propertyChanged",
      handler: expect.any(Function),
    });

    const internalHandler = t.onNotification.mock.calls[0]![0].handler as (
      params: unknown,
    ) => void;
    const payload: PropertyChangedNotification = {
      interestName: "i",
      objectName: "o",
      values: [{ propertyName: "p", value: "1" }],
      timestamp: "2026-05-24T12:00:00Z",
    };
    internalHandler(payload);
    expect(userHandler).toHaveBeenCalledWith(payload);

    expect(typeof cancel).toBe("function");
  });

  it("onEventTriggered routes through 'eventTriggered'", () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    const userHandler = vi.fn();
    client.onEventTriggered(userHandler);
    expect(t.onNotification).toHaveBeenCalledWith({
      method: "eventTriggered",
      handler: expect.any(Function),
    });

    const internalHandler = t.onNotification.mock.calls[0]![0].handler as (
      params: unknown,
    ) => void;
    const payload: EventTriggeredNotification = {
      interestName: "i",
      objectName: "o",
      eventName: "e",
      args: "[]",
      timestamp: "2026-05-24T12:00:00Z",
    };
    internalHandler(payload);
    expect(userHandler).toHaveBeenCalledWith(payload);
  });

  it("onInterestUpdate routes through 'interestUpdate'", () => {
    const t = makeRecordingTransport();
    const client = new JsonRpcClient(t.transport);
    const userHandler = vi.fn();
    client.onInterestUpdate(userHandler);
    expect(t.onNotification).toHaveBeenCalledWith({
      method: "interestUpdate",
      handler: expect.any(Function),
    });

    const internalHandler = t.onNotification.mock.calls[0]![0].handler as (
      params: unknown,
    ) => void;
    const payload: InterestUpdateNotification = {
      interestName: "i",
      added: [{ objectName: "a", qualifiedClassName: "pkg.A", currentValues: [] }],
      removed: [],
      types: [],
      typeSchemas: "",
    };
    internalHandler(payload);
    expect(userHandler).toHaveBeenCalledWith(payload);
  });
});
