// === leaf_samples.test.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";

import { sampleLeaves } from "../../src/state/leaf_samples.js";

describe("sampleLeaves", () => {
  it("samples an i64 / u64 (a bigint) as a number, so that it can be plotted", () => {
    expect(sampleLeaves(42n, "count")).toEqual([["count", 42, null]]);
    expect(sampleLeaves({ total: 2n ** 60n }, "")).toEqual([["total", 2 ** 60, null]]);
  });
});
