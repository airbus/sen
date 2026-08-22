// === json_replacer.test.ts ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { jsonReplacer } from "../../src/tools.js";

describe("jsonReplacer", () => {
  it("emits a top-level bigint as its decimal string", () => {
    expect(JSON.stringify(123n, jsonReplacer)).toBe('"123"');
  });

  it("preserves precision for bigints above Number.MAX_SAFE_INTEGER", () => {
    const big = 9007199254740993n;
    expect(JSON.stringify(big, jsonReplacer)).toBe('"9007199254740993"');
  });

  it("walks nested structures (arrays, objects, deep paths)", () => {
    const payload = { object: "whiskers", property: "counter", value: 18014398509481985n };
    expect(JSON.parse(JSON.stringify(payload, jsonReplacer))).toEqual({
      object: "whiskers",
      property: "counter",
      value: "18014398509481985",
    });
  });

  it("leaves regular numbers, strings, booleans, and nulls untouched", () => {
    const payload = { n: 4, s: "hi", b: true, nul: null, arr: [1, 2, 3] };
    expect(JSON.parse(JSON.stringify(payload, jsonReplacer))).toEqual(payload);
  });
});
