// === types.test.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import type { Client, CustomTypeSpec } from "@sen/client";
import { Quantity, Variant } from "@sen/client";

import { defaultFor } from "../../src/core/types.js";

function mockClient(...specs: CustomTypeSpec[]): Client {
  const byName = new Map(specs.map((s) => [s.qualifiedName, s]));
  return {
    getType: (qualifiedName: string) => byName.get(qualifiedName),
  } as unknown as Client;
}

const meters = { name: "meter", abbreviation: "m", category: "length" as const };

describe("defaultFor", () => {
  it("populates all elements for fixed-size arrays and keeps variable sequences empty", () => {
    const pointSpec: CustomTypeSpec = {
      name: "Point",
      qualifiedName: "test.Point",
      description: "",
      data: {
        type: "sen.kernel.StructTypeSpec",
        value: {
          parent: "",
          fields: [
            { name: "x", description: "", type: "f32" },
            { name: "y", description: "", type: "f32" },
          ],
        },
      },
    };
    const fixedArraySpec: CustomTypeSpec = {
      name: "PointPair",
      qualifiedName: "test.PointPair",
      description: "",
      data: {
        type: "sen.kernel.SequenceTypeSpec",
        value: { elementType: "test.Point", maxSize: 2, fixedSize: true },
      },
    };
    const aliasSpec: CustomTypeSpec = {
      name: "AliasedPair",
      qualifiedName: "test.AliasedPair",
      description: "",
      data: {
        type: "sen.kernel.AliasTypeSpec",
        value: { aliasedType: "test.PointPair" },
      },
    };
    const variantSpec: CustomTypeSpec = {
      name: "Choice",
      qualifiedName: "test.Choice",
      description: "",
      data: {
        type: "sen.kernel.VariantTypeSpec",
        value: {
          fields: [{ key: 0, description: "", type: "test.PointPair" }],
        },
      },
    };
    const boundedSeqSpec: CustomTypeSpec = {
      name: "BoundedFloats",
      qualifiedName: "test.BoundedFloats",
      description: "",
      data: {
        type: "sen.kernel.SequenceTypeSpec",
        value: { elementType: "f32", maxSize: 5, fixedSize: false },
      },
    };
    const unboundedSeqSpec: CustomTypeSpec = {
      name: "Floats",
      qualifiedName: "test.Floats",
      description: "",
      data: {
        type: "sen.kernel.SequenceTypeSpec",
        value: { elementType: "f32", maxSize: null, fixedSize: false },
      },
    };

    const client = mockClient(
      pointSpec,
      fixedArraySpec,
      aliasSpec,
      variantSpec,
      boundedSeqSpec,
      unboundedSeqSpec,
    );

    const expected = [
      { x: 0, y: 0 },
      { x: 0, y: 0 },
    ];
    expect(defaultFor(client, "test.PointPair")).toEqual(expected);
    expect(defaultFor(client, "test.AliasedPair")).toEqual(expected);

    const v = defaultFor(client, "test.Choice") as Variant;
    expect(v).toBeInstanceOf(Variant);
    expect(v.value).toEqual(expected);

    expect(defaultFor(client, "test.BoundedFloats")).toEqual([]);
    expect(defaultFor(client, "test.Floats")).toEqual([]);
  });

  it("respects quantity bounds when zero is out of range", () => {
    const makeQty = (
      name: string,
      minValue: number | null,
      maxValue: number | null,
    ): CustomTypeSpec => ({
      name,
      qualifiedName: `test.${name}`,
      description: "",
      data: {
        type: "sen.kernel.QuantityTypeSpec",
        value: {
          elementType: { type: "sen.kernel.RealType", value: "float32Type" },
          unit: meters,
          minValue,
          maxValue,
        },
      },
    });

    const client = mockClient(
      makeQty("Positive", 10, 100),
      makeQty("Negative", -50, -10),
      makeQty("MaxOnly", null, -5),
      makeQty("Centered", -10, 10),
    );

    expect((defaultFor(client, "test.Positive") as Quantity).value).toBe(10);
    expect((defaultFor(client, "test.Negative") as Quantity).value).toBe(-50);
    expect((defaultFor(client, "test.MaxOnly") as Quantity).value).toBe(-5);
    expect((defaultFor(client, "test.Centered") as Quantity).value).toBe(0);
  });
});
