// === leaf_samples.test.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";

import { sampleLeaves, timeMsAt, type BufferView } from "../../src/state/leaf_samples.js";

describe("sampleLeaves", () => {
  it("samples an i64 / u64 (a bigint) as a number, so that it can be plotted", () => {
    expect(sampleLeaves(42n, "count")).toEqual([["count", 42, null]]);
    expect(sampleLeaves({ total: 2n ** 60n }, "")).toEqual([["total", 2 ** 60, null]]);
  });
});

describe("timeMsAt", () => {
  const viewOf = (at: number[], tServer: number[]): BufferView => ({
    size: at.length,
    at: Float64Array.from(at),
    tServer: Float64Array.from(tServer),
    vKind: null,
    vNum: null,
    vBool: null,
    vStr: null,
    vGap: null,
    unit: null,
  });

  it("plots arrival time, so a source carrying its own clock does not move the axis", () => {
    const view = viewOf([1_000, 2_000], [1_700_000_000_000, 1_700_000_001_000]);
    expect(timeMsAt(view, 0)).toBe(1_000);
    expect(timeMsAt(view, 1)).toBe(2_000);
  });

  it("keeps one clock across samples that do and do not carry a source time", () => {
    const view = viewOf([1_000, 2_000], [NaN, 1_700_000_000_000]);
    expect(timeMsAt(view, 1) - timeMsAt(view, 0)).toBe(1_000);
  });
});
