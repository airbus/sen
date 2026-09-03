// === type_cache.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";
import { TypeCache } from "../src/internal/type_cache.js";
import { TransportError } from "../src/errors.js";
import type { CustomTypeSpec } from "../src/index.js";

function makeSpec(qualifiedName: string, description = ""): CustomTypeSpec {
  return {
    name: qualifiedName.split(".").pop() ?? qualifiedName,
    qualifiedName,
    description,
    data: { type: "sen.kernel.AliasTypeSpec", value: { aliasedType: "string" } },
  };
}

describe("TypeCache", () => {
  it("get returns undefined for unknown names", () => {
    const cache = new TypeCache();
    expect(cache.get("pkg.X")).toBeUndefined();
    expect(cache.has("pkg.X")).toBe(false);
    expect(cache.size()).toBe(0);
  });

  it("set then get returns the same spec", () => {
    const cache = new TypeCache();
    const spec = makeSpec("pkg.X", "first");
    cache.set(spec);
    expect(cache.get("pkg.X")).toBe(spec);
    expect(cache.has("pkg.X")).toBe(true);
    expect(cache.size()).toBe(1);
  });

  it("set overwrites an existing entry with the same qualifiedName", () => {
    const cache = new TypeCache();
    cache.set(makeSpec("pkg.X", "first"));
    cache.set(makeSpec("pkg.X", "second"));
    expect(cache.get("pkg.X")?.description).toBe("second");
    expect(cache.size()).toBe(1);
  });

  it("setMany inserts every spec in the input", () => {
    const cache = new TypeCache();
    cache.setMany([makeSpec("pkg.A"), makeSpec("pkg.B"), makeSpec("pkg.C")]);
    expect(cache.size()).toBe(3);
    expect(cache.has("pkg.A")).toBe(true);
    expect(cache.has("pkg.B")).toBe(true);
    expect(cache.has("pkg.C")).toBe(true);
  });

  it("clear empties the cache", () => {
    const cache = new TypeCache();
    cache.set(makeSpec("pkg.X"));
    cache.clear();
    expect(cache.size()).toBe(0);
  });

  it("setSchemasFromBlob routes parse errors through reportError", () => {
    const reportError = vi.fn();
    const cache = new TypeCache(reportError);
    cache.setSchemasFromBlob("{not-json");
    expect(reportError).toHaveBeenCalledTimes(1);
    const err = reportError.mock.calls[0]![0];
    expect(err).toBeInstanceOf(TransportError);
    expect((err as Error).message).toMatch(/typeSchemas blob failed to parse/);
  });

  it("setSchemasFromBlob is a no-op for empty strings without reporting", () => {
    const reportError = vi.fn();
    const cache = new TypeCache(reportError);
    cache.setSchemasFromBlob("");
    expect(reportError).not.toHaveBeenCalled();
  });
});
