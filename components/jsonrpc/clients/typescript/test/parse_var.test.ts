// === parse_var.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import { parseVar } from "../src/internal/var_codec.js";
import { TypeCache } from "../src/internal/type_cache.js";
import { Quantity, Variant, TransportError } from "../src/index.js";
import type { CustomTypeSpec } from "../src/index.js";

/** Seed a cache with the given specs and return it. */
function cacheOf(...specs: CustomTypeSpec[]): TypeCache {
  const cache = new TypeCache();
  for (const s of specs) cache.set(s);
  return cache;
}

const meters = { name: "meter", abbreviation: "m", category: "length" as const };

describe("parseVar -- built-in primitives", () => {
  it("string passes through verbatim", () => {
    expect(parseVar("string", "hello", new TypeCache())).toBe("hello");
  });
  it("bool passes through verbatim", () => {
    expect(parseVar("bool", true, new TypeCache())).toBe(true);
  });
  it("small integers + floats pass through verbatim as number", () => {
    expect(parseVar("i32", 42, new TypeCache())).toBe(42);
    expect(parseVar("u32", 999, new TypeCache())).toBe(999);
    expect(parseVar("f64", 3.14, new TypeCache())).toBe(3.14);
  });
  it("i64/u64 arrive as decimal strings and decode to bigint", () => {
    expect(parseVar("i64", "-9007199254740993", new TypeCache())).toBe(-9007199254740993n);
    expect(parseVar("u64", "18446744073709551615", new TypeCache())).toBe(18446744073709551615n);
  });
  it("i64/u64 reject a JSON number (would silently truncate past 2^53)", () => {
    expect(() => parseVar("u64", 42, new TypeCache())).toThrow(TransportError);
    expect(() => parseVar("u64", 42, new TypeCache())).toThrow(/decimal string/);
  });
  it("i64/u64 reject malformed decimal strings", () => {
    expect(() => parseVar("i64", "not-a-number", new TypeCache())).toThrow(TransportError);
    expect(() => parseVar("u64", "1.5", new TypeCache())).toThrow(TransportError);
  });
  it("TimeStamp + Duration are strings on the wire", () => {
    expect(parseVar("TimeStamp", "2026-05-24T12:00:00Z", new TypeCache())).toBe(
      "2026-05-24T12:00:00Z",
    );
    expect(parseVar("Duration", "PT1.5S", new TypeCache())).toBe("PT1.5S");
  });
});

describe("parseVar -- missing spec invariant", () => {
  it("throws TransportError when a referenced custom type has no cached spec", () => {
    expect(() => parseVar("pkg.Missing", {}, new TypeCache())).toThrow(TransportError);
    expect(() => parseVar("pkg.Missing", {}, new TypeCache())).toThrow(/no type spec/);
  });
});

describe("parseVar -- enum", () => {
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

  it("returns enum values as bare strings (decision B)", () => {
    expect(parseVar("demo.Color", "red", cacheOf(colorSpec))).toBe("red");
  });

  it("throws if the wire value is not a string", () => {
    expect(() => parseVar("demo.Color", 0, cacheOf(colorSpec))).toThrow(TransportError);
  });

  it("throws if the wire value isn't one of the spec's enumerators", () => {
    expect(() => parseVar("demo.Color", "crimson", cacheOf(colorSpec))).toThrow(
      /enum "demo\.Color" has no enumerator "crimson"/,
    );
  });
});

describe("parseVar -- Quantity", () => {
  const altSpec: CustomTypeSpec = {
    name: "Altitude",
    qualifiedName: "demo.Altitude",
    description: "",
    data: {
      type: "sen.kernel.QuantityTypeSpec",
      value: {
        elementType: { type: "sen.kernel.RealType", value: "float64Type" },
        unit: meters,
        minValue: 0,
        maxValue: 50000,
      },
    },
  };

  it("constructs a Quantity carrying unit and bounds from the spec", () => {
    const v = parseVar("demo.Altitude", 12345.6, cacheOf(altSpec));
    expect(v).toBeInstanceOf(Quantity);
    const q = v as Quantity;
    expect(q.value).toBe(12345.6);
    expect(q.unit.abbreviation).toBe("m");
    expect(q.minValue).toBe(0);
    expect(q.maxValue).toBe(50000);
  });

  it("throws if the wire value is not a number", () => {
    expect(() => parseVar("demo.Altitude", "12345", cacheOf(altSpec))).toThrow(TransportError);
  });
});

describe("parseVar -- sequence", () => {
  const u32List: CustomTypeSpec = {
    name: "U32List",
    qualifiedName: "demo.U32List",
    description: "",
    data: {
      type: "sen.kernel.SequenceTypeSpec",
      value: { elementType: "u32", maxSize: null, fixedSize: false },
    },
  };

  it("recursively parses each element against elementType", () => {
    const v = parseVar("demo.U32List", [1, 2, 3], cacheOf(u32List));
    expect(v).toEqual([1, 2, 3]);
  });

  it("throws if the wire value is not an array", () => {
    expect(() => parseVar("demo.U32List", "not-an-array", cacheOf(u32List))).toThrow(
      TransportError,
    );
  });
});

describe("parseVar -- struct", () => {
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

  it("parses each declared field by name", () => {
    const v = parseVar("demo.Point", { x: 1.5, y: -2.5 }, cacheOf(pointSpec));
    expect(v).toEqual({ x: 1.5, y: -2.5 });
  });

  it("throws if the wire value is not an object", () => {
    expect(() => parseVar("demo.Point", [1, 2], cacheOf(pointSpec))).toThrow(TransportError);
  });
});

describe("parseVar -- struct with parent chain", () => {
  const baseSpec: CustomTypeSpec = {
    name: "Base",
    qualifiedName: "demo.Base",
    description: "",
    data: {
      type: "sen.kernel.StructTypeSpec",
      value: {
        parent: "",
        fields: [{ name: "id", description: "", type: "string" }],
      },
    },
  };
  const childSpec: CustomTypeSpec = {
    name: "Child",
    qualifiedName: "demo.Child",
    description: "",
    data: {
      type: "sen.kernel.StructTypeSpec",
      value: {
        parent: "demo.Base",
        fields: [{ name: "value", description: "", type: "i32" }],
      },
    },
  };

  it("includes parent fields in the parsed result", () => {
    const v = parseVar("demo.Child", { id: "x", value: 7 }, cacheOf(baseSpec, childSpec));
    expect(v).toEqual({ id: "x", value: 7 });
  });

  it("throws if a parent struct isn't cached", () => {
    expect(() => parseVar("demo.Child", { id: "x", value: 7 }, cacheOf(childSpec))).toThrow(
      /parent struct "demo\.Base" not cached/,
    );
  });
});

describe("parseVar -- variant", () => {
  const sleepingSpec: CustomTypeSpec = {
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
  const doingNothingSpec: CustomTypeSpec = {
    name: "DoingNothing",
    qualifiedName: "school.DoingNothing",
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
      value: {
        fields: [
          { key: 0, description: "", type: "school.DoingNothing" },
          { key: 1, description: "", type: "school.Sleeping" },
        ],
      },
    },
  };

  it("matches the arm by qualified name", () => {
    const v = parseVar(
      "school.StudentStatus",
      { type: "school.Sleeping", value: { since: "2026-05-24T12:00:00Z" } },
      cacheOf(sleepingSpec, doingNothingSpec, statusSpec),
    );
    expect(v).toBeInstanceOf(Variant);
    const sv = v as Variant;
    expect(sv.type).toBe("school.Sleeping");
    expect(sv.value).toEqual({ since: "2026-05-24T12:00:00Z" });
  });

  it("throws if the tag doesn't match any arm", () => {
    expect(() =>
      parseVar(
        "school.StudentStatus",
        { type: "school.Walking", value: {} },
        cacheOf(sleepingSpec, doingNothingSpec, statusSpec),
      ),
    ).toThrow(/no arm matching tag "school\.Walking"/);
  });

  it("throws if the 'type' tag is not a string", () => {
    expect(() =>
      parseVar(
        "school.StudentStatus",
        { type: 0, value: {} },
        cacheOf(sleepingSpec, doingNothingSpec, statusSpec),
      ),
    ).toThrow(/requires string "type" tag/);
  });
});

describe("parseVar -- alias + optional", () => {
  const aliasSpec: CustomTypeSpec = {
    name: "Speed",
    qualifiedName: "demo.Speed",
    description: "",
    data: { type: "sen.kernel.AliasTypeSpec", value: { aliasedType: "f64" } },
  };
  const maybeIdSpec: CustomTypeSpec = {
    name: "MaybeId",
    qualifiedName: "demo.MaybeId",
    description: "",
    data: { type: "sen.kernel.OptionalTypeSpec", value: { type: "string" } },
  };

  it("alias delegates to its aliased type", () => {
    expect(parseVar("demo.Speed", 250.5, cacheOf(aliasSpec))).toBe(250.5);
  });

  it("optional returns null when the wire value is null", () => {
    expect(parseVar("demo.MaybeId", null, cacheOf(maybeIdSpec))).toBeNull();
  });

  it("optional delegates to the inner type when non-null", () => {
    expect(parseVar("demo.MaybeId", "abc", cacheOf(maybeIdSpec))).toBe("abc");
  });
});

describe("parseVar -- ClassTypeSpec is not a value type", () => {
  const classSpec: CustomTypeSpec = {
    name: "Person",
    qualifiedName: "school.Person",
    description: "",
    data: {
      type: "sen.kernel.ClassTypeSpec",
      value: {
        properties: [],
        methods: [],
        events: [],
        constructor: {
          name: "Person",
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

  it("throws TransportError if asked to parse a class spec as a value", () => {
    expect(() => parseVar("school.Person", {}, cacheOf(classSpec))).toThrow(
      /not a value type/,
    );
  });
});
