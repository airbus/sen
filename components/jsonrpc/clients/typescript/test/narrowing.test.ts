// === narrowing.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import {
  isPrimitive,
  isQuantity,
  isSequence,
  isStruct,
  isVariant,
} from "../src/narrowing.js";
import { Quantity, Variant } from "../src/index.js";

const meters = { name: "meter", abbreviation: "m", category: "length" as const };

describe("narrowing helpers", () => {
  it("isQuantity identifies Quantity instances and rejects plain numbers", () => {
    expect(isQuantity(new Quantity(10, meters))).toBe(true);
    expect(isQuantity(10)).toBe(false);
    expect(isQuantity({ value: 10, unit: meters })).toBe(false);
  });

  it("isVariant identifies Variant instances and rejects plain objects", () => {
    expect(isVariant(new Variant("X", { a: 1 }))).toBe(true);
    expect(isVariant({ type: "X", value: { a: 1 } })).toBe(false);
    expect(isVariant("plain")).toBe(false);
  });

  it("isStruct identifies plain objects, excluding runtime classes and arrays", () => {
    expect(isStruct({ a: 1, b: "s" })).toBe(true);
    expect(isStruct([1, 2, 3])).toBe(false);
    expect(isStruct(null)).toBe(false);
    expect(isStruct(new Quantity(10, meters))).toBe(false);
    expect(isStruct(new Variant("X", null))).toBe(false);
    expect(isStruct("scalar")).toBe(false);
  });

  it("isSequence identifies arrays", () => {
    expect(isSequence([1, 2, 3])).toBe(true);
    expect(isSequence([])).toBe(true);
    expect(isSequence("not-array")).toBe(false);
    expect(isSequence({ length: 3 })).toBe(false);
  });

  it("isPrimitive identifies string / number / boolean / null", () => {
    expect(isPrimitive("hello")).toBe(true);
    expect(isPrimitive(42)).toBe(true);
    expect(isPrimitive(false)).toBe(true);
    expect(isPrimitive(null)).toBe(true);
    expect(isPrimitive([])).toBe(false);
    expect(isPrimitive({})).toBe(false);
    expect(isPrimitive(new Quantity(10, meters))).toBe(false);
    expect(isPrimitive(new Variant("X", null))).toBe(false);
  });
});
