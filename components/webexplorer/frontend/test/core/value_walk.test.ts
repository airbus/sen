// === value_walk.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";

import { extractNumericAt } from "../../src/core/value_walk.js";

describe("extractNumericAt", () => {
  it("reads an i64 / u64 (a bigint) as a number", () => {
    expect(extractNumericAt({ count: 7n }, "count")).toBe(7);
    expect(extractNumericAt(-3n, "")).toBe(-3);
  });
});
