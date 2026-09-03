// === encode_var.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import { encodeVar, encodeVarToWire } from "../src/internal/var_codec.js";
import { TypeCache } from "../src/internal/type_cache.js";
import { Quantity, Variant, TransportError } from "../src/index.js";
import type { CustomTypeSpec } from "../src/index.js";

function cacheOf(...specs: CustomTypeSpec[]): TypeCache {
  const cache = new TypeCache();
  for (const s of specs) cache.set(s);
  return cache;
}

const meters = { name: "meter", abbreviation: "m", category: "length" as const };

describe("encodeVar -- built-in primitives", () => {
  it("passes primitives through verbatim", () => {
    expect(encodeVar("string", "x", new TypeCache())).toBe("x");
    expect(encodeVar("bool", true, new TypeCache())).toBe(true);
    expect(encodeVar("i32", 42, new TypeCache())).toBe(42);
    expect(encodeVar("f64", 3.14, new TypeCache())).toBe(3.14);
    expect(encodeVar("TimeStamp", "2026-05-24", new TypeCache())).toBe("2026-05-24");
  });

  it("throws when the value doesn't match the declared primitive", () => {
    expect(() => encodeVar("string", 42, new TypeCache())).toThrow(TransportError);
    expect(() => encodeVar("i32", "42", new TypeCache())).toThrow(TransportError);
    expect(() => encodeVar("bool", 1, new TypeCache())).toThrow(TransportError);
  });

  it("encodeVarToWire stringifies the encoded form", () => {
    expect(encodeVarToWire("string", "hello", new TypeCache())).toBe('"hello"');
    expect(encodeVarToWire("i32", 7, new TypeCache())).toBe("7");
  });

  it("i64/u64 encode bigint as a decimal-string JSON value", () => {
    expect(encodeVar("i64", -9007199254740993n, new TypeCache())).toBe("-9007199254740993");
    expect(encodeVar("u64", 18446744073709551615n, new TypeCache())).toBe("18446744073709551615");
    expect(encodeVarToWire("u64", 42n, new TypeCache())).toBe('"42"');
  });

  it("i64/u64 accept safe-integer numbers, reject unsafe or non-integer numbers", () => {
    expect(encodeVar("i64", 42, new TypeCache())).toBe("42");
    expect(() => encodeVar("u64", 1.5, new TypeCache())).toThrow(/non-integer/);
    expect(() => encodeVar("u64", Number.MAX_SAFE_INTEGER + 1, new TypeCache())).toThrow(
      /safe-integer range/,
    );
  });

  it("u64 rejects negative bigints", () => {
    expect(() => encodeVar("u64", -1n, new TypeCache())).toThrow(/negative/);
  });
});

describe("encodeVar -- enum", () => {
  const colorSpec: CustomTypeSpec = {
    name: "Color",
    qualifiedName: "demo.Color",
    description: "",
    data: {
      type: "sen.kernel.EnumTypeSpec",
      value: {
        storageType: "uint8Type",
        enums: [
          { name: "red", key: 0, description: "" },
          { name: "green", key: 1, description: "" },
          { name: "blue", key: 2, description: "" },
        ],
      },
    },
  };

  it("passes a known enumerator through verbatim", () => {
    expect(encodeVar("demo.Color", "red", cacheOf(colorSpec))).toBe("red");
  });

  it("throws if the value is not a string", () => {
    expect(() => encodeVar("demo.Color", 0, cacheOf(colorSpec))).toThrow(TransportError);
  });

  it("throws if the value isn't one of the spec's enumerators", () => {
    expect(() => encodeVar("demo.Color", "crimson", cacheOf(colorSpec))).toThrow(
      /enum "demo\.Color" has no enumerator "crimson"/,
    );
  });
});

describe("encodeVar -- Quantity", () => {
  const altSpec: CustomTypeSpec = {
    name: "Altitude",
    qualifiedName: "demo.Altitude",
    description: "",
    data: {
      type: "sen.kernel.QuantityTypeSpec",
      value: {
        elementType: { type: "sen.kernel.RealType", value: "float64Type" },
        unit: meters,
        minValue: null,
        maxValue: null,
      },
    },
  };

  it("unwraps a Quantity instance to its .value", () => {
    const q = new Quantity(1234, meters);
    expect(encodeVar("demo.Altitude", q, cacheOf(altSpec))).toBe(1234);
  });

  it("accepts a plain number as the wire-shape equivalent", () => {
    expect(encodeVar("demo.Altitude", 999.5, cacheOf(altSpec))).toBe(999.5);
  });

  it("throws on incompatible value", () => {
    expect(() => encodeVar("demo.Altitude", "high", cacheOf(altSpec))).toThrow(TransportError);
  });

  it("rejects a Quantity whose unit does not match the spec", () => {
    const milesPerHour = { name: "mile_per_hour", abbreviation: "mph", category: "velocity" as const };
    const q = new Quantity(10, milesPerHour);
    expect(() => encodeVar("demo.Altitude", q, cacheOf(altSpec))).toThrow(
      /quantity "demo\.Altitude" expects unit "m", got "mph"/,
    );
  });

  it("rejects a numeric value outside the spec's bounds", () => {
    const boundedSpec: CustomTypeSpec = {
      name: "BoundedAltitude",
      qualifiedName: "demo.BoundedAltitude",
      description: "",
      data: {
        type: "sen.kernel.QuantityTypeSpec",
        value: {
          elementType: { type: "sen.kernel.RealType", value: "float64Type" },
          unit: meters,
          minValue: 0,
          maxValue: 100,
        },
      },
    };
    expect(() => encodeVar("demo.BoundedAltitude", -1, cacheOf(boundedSpec))).toThrow(
      /below minimum 0/,
    );
    expect(() => encodeVar("demo.BoundedAltitude", 200, cacheOf(boundedSpec))).toThrow(
      /above maximum 100/,
    );
    expect(encodeVar("demo.BoundedAltitude", 50, cacheOf(boundedSpec))).toBe(50);
  });
});

describe("encodeVar -- Variant", () => {
  const armSpec: CustomTypeSpec = {
    name: "Sleeping",
    qualifiedName: "school.Sleeping",
    description: "",
    data: {
      type: "sen.kernel.StructTypeSpec",
      value: {
        parent: "",
        fields: [{ name: "since", description: "", type: "TimeStamp" }],
      },
    },
  };
  const statusSpec: CustomTypeSpec = {
    name: "StudentStatus",
    qualifiedName: "school.StudentStatus",
    description: "",
    data: {
      type: "sen.kernel.VariantTypeSpec",
      value: { fields: [{ key: 0, description: "", type: "school.Sleeping" }] },
    },
  };

  it("accepts a Variant instance and writes {type, value}", () => {
    const v = new Variant("school.Sleeping", { since: "2026-05-24" });
    const encoded = encodeVar("school.StudentStatus", v, cacheOf(armSpec, statusSpec));
    expect(encoded).toEqual({ type: "school.Sleeping", value: { since: "2026-05-24" } });
  });

  it("accepts a plain {type, value} object", () => {
    const encoded = encodeVar(
      "school.StudentStatus",
      { type: "school.Sleeping", value: { since: "2026-05-24" } },
      cacheOf(armSpec, statusSpec),
    );
    expect(encoded).toEqual({ type: "school.Sleeping", value: { since: "2026-05-24" } });
  });

  it("throws when the tag doesn't match any arm", () => {
    expect(() =>
      encodeVar(
        "school.StudentStatus",
        { type: "school.Walking", value: {} },
        cacheOf(armSpec, statusSpec),
      ),
    ).toThrow(/no arm matching tag "school.Walking"/);
  });
});

describe("encodeVar -- sequence + struct + optional + alias", () => {
  const pointSpec: CustomTypeSpec = {
    name: "Point",
    qualifiedName: "demo.Point",
    description: "",
    data: {
      type: "sen.kernel.StructTypeSpec",
      value: {
        parent: "",
        fields: [
          { name: "x", description: "", type: "f64" },
          { name: "y", description: "", type: "f64" },
        ],
      },
    },
  };
  const seqSpec: CustomTypeSpec = {
    name: "Points",
    qualifiedName: "demo.Points",
    description: "",
    data: {
      type: "sen.kernel.SequenceTypeSpec",
      value: { elementType: "demo.Point", maxSize: null, fixedSize: false },
    },
  };
  const aliasSpec: CustomTypeSpec = {
    name: "Speed",
    qualifiedName: "demo.Speed",
    description: "",
    data: { type: "sen.kernel.AliasTypeSpec", value: { aliasedType: "f64" } },
  };
  const optionalSpec: CustomTypeSpec = {
    name: "MaybeName",
    qualifiedName: "demo.MaybeName",
    description: "",
    data: { type: "sen.kernel.OptionalTypeSpec", value: { type: "string" } },
  };

  it("struct encodes each declared field", () => {
    expect(
      encodeVar("demo.Point", { x: 1, y: 2 }, cacheOf(pointSpec)),
    ).toEqual({ x: 1, y: 2 });
  });

  it("sequence encodes each element", () => {
    expect(
      encodeVar(
        "demo.Points",
        [
          { x: 1, y: 2 },
          { x: 3, y: 4 },
        ],
        cacheOf(pointSpec, seqSpec),
      ),
    ).toEqual([
      { x: 1, y: 2 },
      { x: 3, y: 4 },
    ]);
  });

  it("alias delegates to its aliased type", () => {
    expect(encodeVar("demo.Speed", 250, cacheOf(aliasSpec))).toBe(250);
  });

  it("optional emits null for null or undefined input", () => {
    expect(encodeVar("demo.MaybeName", null, cacheOf(optionalSpec))).toBeNull();
    expect(encodeVar("demo.MaybeName", undefined, cacheOf(optionalSpec))).toBeNull();
  });

  it("optional delegates to inner type when non-null", () => {
    expect(encodeVar("demo.MaybeName", "abc", cacheOf(optionalSpec))).toBe("abc");
  });
});

describe("encodeVar -- missing-spec invariant", () => {
  it("throws TransportError when a referenced custom type has no cached spec", () => {
    expect(() => encodeVar("pkg.Missing", {}, new TypeCache())).toThrow(/no type spec/);
  });
});
