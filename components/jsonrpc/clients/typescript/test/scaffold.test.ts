// === scaffold.test.ts ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import {
  Quantity,
  Variant,
  numberFromExact,
  type CustomTypeSpec,
  type PropertyValueList,
  type UnitInfo,
} from "../src/index.js";
import type { MemberSelector, SubscribeBlock } from "../src/generated/index.js";

describe("Stage 1 scaffold", () => {
  it("Quantity carries value + unit and formats sensibly", () => {
    const meters: UnitInfo = { name: "meter", abbreviation: "m", category: "length" };
    const q = new Quantity(42, meters, 0, 1000);
    expect(q.value).toBe(42);
    expect(q.unit.abbreviation).toBe("m");
    expect(q.minValue).toBe(0);
    expect(q.maxValue).toBe(1000);
    expect(q.toString()).toBe("42 m");
  });

  it("Quantity equality respects value, unit, and bounds", () => {
    const meters: UnitInfo = { name: "meter", abbreviation: "m", category: "length" };
    const a = new Quantity(10, meters, 0, 100);
    const b = new Quantity(10, meters, 0, 100);
    const c = new Quantity(10, meters, 0, 200);
    expect(a.equals(b)).toBe(true);
    expect(a.equals(c)).toBe(false);
  });

  it("Variant preserves discriminant and payload", () => {
    const v = new Variant("Sleeping", { since: "2026-05-24T12:00:00Z", snortingVolume: 0.3 });
    expect(v.type).toBe("Sleeping");
    expect(v.value).toEqual({ since: "2026-05-24T12:00:00Z", snortingVolume: 0.3 });
  });

  it("MemberSelector discriminated union narrows on `type`", () => {
    const wildcard: MemberSelector = { type: "sen.components.jsonrpc.WildcardSelection", value: {} };
    const named: MemberSelector = {
      type: "sen.components.jsonrpc.NamedSelection",
      value: { memberNames: ["altitude", "heading"] },
    };

    function describe(sel: MemberSelector): string {
      switch (sel.type) {
        case "sen.components.jsonrpc.WildcardSelection":
          return "all";
        case "sen.components.jsonrpc.NamedSelection":
          return sel.value.memberNames.join(",");
      }
    }

    expect(describe(wildcard)).toBe("all");
    expect(describe(named)).toBe("altitude,heading");
  });

  it("SubscribeBlock optionals accept null", () => {
    const block: SubscribeBlock = {
      properties: { type: "sen.components.jsonrpc.WildcardSelection", value: {} },
      events: null,
      maxRateHz: 30,
    };
    expect(block.events).toBeNull();
    expect(block.maxRateHz).toBe(30);
  });

  it("PropertyValueList is a sequence of string-encoded values", () => {
    const values: PropertyValueList = [
      { propertyName: "altitude", value: "12345.6" },
      { propertyName: "label", value: "\"hello\"" },
    ];
    expect(values).toHaveLength(2);
    expect(values[0]?.value).toBe("12345.6");
  });

  it("CustomTypeSpec discriminated `data` narrows by arm", () => {
    const spec: CustomTypeSpec = {
      name: "Altitude",
      qualifiedName: "demo.Altitude",
      description: "Altitude in meters",
      data: {
        type: "sen.kernel.QuantityTypeSpec",
        value: {
          elementType: { type: "sen.kernel.RealType", value: "float64Type" },
          unit: { name: "meter", abbreviation: "m", category: "length" },
          minValue: 0,
          maxValue: 50000,
        },
      },
    };
    if (spec.data.type === "sen.kernel.QuantityTypeSpec") {
      expect(spec.data.value.unit.abbreviation).toBe("m");
    } else {
      throw new Error("expected QuantityTypeSpec arm");
    }
  });

  it("numberFromExact narrows a safe bigint and throws past MAX_SAFE_INTEGER", () => {
    expect(numberFromExact(42n)).toBe(42);
    expect(numberFromExact(BigInt(Number.MAX_SAFE_INTEGER))).toBe(Number.MAX_SAFE_INTEGER);
    expect(() => numberFromExact(BigInt(Number.MAX_SAFE_INTEGER) + 1n)).toThrow(RangeError);
    expect(() => numberFromExact(BigInt(Number.MIN_SAFE_INTEGER) - 1n, "tickCount")).toThrow(
      /tickCount/,
    );
  });
});
