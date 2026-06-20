// === notification_validators.test.ts =================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import {
  isEventTriggeredNotification,
  isInterestUpdateNotification,
  isPropertyChangedNotification,
  isTopologyChangedNotification,
} from "../src/internal/notification_validators.js";

describe("isPropertyChangedNotification", () => {
  it("accepts a well-formed payload", () => {
    expect(
      isPropertyChangedNotification({
        interestName: "i1",
        objectName: "a1",
        values: [{ propertyName: "x", value: "1" }],
        timestamp: "2026-05-25",
      }),
    ).toBe(true);
  });

  it("rejects missing fields", () => {
    expect(isPropertyChangedNotification({})).toBe(false);
    expect(
      isPropertyChangedNotification({
        interestName: "i1",
        objectName: "a1",
        timestamp: "2026-05-25",
      }),
    ).toBe(false);
  });

  it("rejects wrong field types", () => {
    expect(
      isPropertyChangedNotification({
        interestName: 42,
        objectName: "a1",
        values: [],
        timestamp: "2026-05-25",
      }),
    ).toBe(false);
    expect(
      isPropertyChangedNotification({
        interestName: "i1",
        objectName: "a1",
        values: "not-an-array",
        timestamp: "2026-05-25",
      }),
    ).toBe(false);
  });

  it("rejects null / non-object roots", () => {
    expect(isPropertyChangedNotification(null)).toBe(false);
    expect(isPropertyChangedNotification("string")).toBe(false);
    expect(isPropertyChangedNotification([])).toBe(false);
  });
});

describe("isEventTriggeredNotification", () => {
  it("accepts a well-formed payload", () => {
    expect(
      isEventTriggeredNotification({
        interestName: "i1",
        objectName: "a1",
        eventName: "tick",
        args: "[]",
        timestamp: "2026-05-25",
      }),
    ).toBe(true);
  });

  it("rejects when args is not a string (would have been a wire-shape bug)", () => {
    expect(
      isEventTriggeredNotification({
        interestName: "i1",
        objectName: "a1",
        eventName: "tick",
        args: [],
        timestamp: "2026-05-25",
      }),
    ).toBe(false);
  });
});

describe("isInterestUpdateNotification", () => {
  it("accepts a well-formed payload", () => {
    expect(
      isInterestUpdateNotification({
        interestName: "i1",
        added: [],
        removed: [],
        types: [], typeSchemas: "",
      }),
    ).toBe(true);
  });

  it("rejects when arrays are missing or replaced with other shapes", () => {
    expect(
      isInterestUpdateNotification({
        interestName: "i1",
        added: {},
        removed: [],
        types: [], typeSchemas: "",
      }),
    ).toBe(false);
    expect(
      isInterestUpdateNotification({
        interestName: "i1",
        added: [],
        removed: [],
      }),
    ).toBe(false);
  });
});

describe("isTopologyChangedNotification", () => {
  it("accepts an empty sessions array", () => {
    expect(isTopologyChangedNotification({ sessions: [] })).toBe(true);
  });

  it("accepts a populated sessions array", () => {
    expect(
      isTopologyChangedNotification({
        sessions: [{ name: "local", buses: ["jsonrpc"] }],
      }),
    ).toBe(true);
  });

  it("rejects when sessions is missing or not an array", () => {
    expect(isTopologyChangedNotification({})).toBe(false);
    expect(isTopologyChangedNotification({ sessions: "no" })).toBe(false);
    expect(isTopologyChangedNotification(null)).toBe(false);
  });
});
