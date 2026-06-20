// === subscribe_codec.test.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import { toWireSubscribeBlock } from "../src/internal/subscribe_codec.js";

describe("toWireSubscribeBlock", () => {
  it("returns null when subscribe is undefined", () => {
    expect(toWireSubscribeBlock(undefined)).toBeNull();
  });

  it("emits wildcard selector for properties: '*'", () => {
    expect(toWireSubscribeBlock({ properties: "*" })).toEqual({
      properties: { type: "sen.components.jsonrpc.WildcardSelection", value: {} },
      events: null,
      maxRateHz: null,
    });
  });

  it("emits named selector for properties: [...]", () => {
    expect(toWireSubscribeBlock({ properties: ["altitude", "speed"] })).toEqual({
      properties: {
        type: "sen.components.jsonrpc.NamedSelection",
        value: { memberNames: ["altitude", "speed"] },
      },
      events: null,
      maxRateHz: null,
    });
  });

  it("emits wildcard selector for events: '*'", () => {
    expect(toWireSubscribeBlock({ events: "*" })).toEqual({
      properties: null,
      events: { type: "sen.components.jsonrpc.WildcardSelection", value: {} },
      maxRateHz: null,
    });
  });

  it("emits named selector for events: [...]", () => {
    expect(toWireSubscribeBlock({ events: ["landed", "takenOff"] })).toEqual({
      properties: null,
      events: {
        type: "sen.components.jsonrpc.NamedSelection",
        value: { memberNames: ["landed", "takenOff"] },
      },
      maxRateHz: null,
    });
  });

  it("passes maxRateHz through", () => {
    expect(toWireSubscribeBlock({ properties: "*", maxRateHz: 10 })).toEqual({
      properties: { type: "sen.components.jsonrpc.WildcardSelection", value: {} },
      events: null,
      maxRateHz: 10,
    });
  });

  it("combines properties, events, and maxRateHz", () => {
    expect(
      toWireSubscribeBlock({
        properties: ["altitude"],
        events: "*",
        maxRateHz: 5,
      }),
    ).toEqual({
      properties: {
        type: "sen.components.jsonrpc.NamedSelection",
        value: { memberNames: ["altitude"] },
      },
      events: { type: "sen.components.jsonrpc.WildcardSelection", value: {} },
      maxRateHz: 5,
    });
  });

  it("handles an empty object as 'subscribe to nothing' (all fields null)", () => {
    expect(toWireSubscribeBlock({})).toEqual({
      properties: null,
      events: null,
      maxRateHz: null,
    });
  });

  it("handles an empty selector array as 'subscribe to no named members'", () => {
    expect(toWireSubscribeBlock({ properties: [] })).toEqual({
      properties: {
        type: "sen.components.jsonrpc.NamedSelection",
        value: { memberNames: [] },
      },
      events: null,
      maxRateHz: null,
    });
  });
});
