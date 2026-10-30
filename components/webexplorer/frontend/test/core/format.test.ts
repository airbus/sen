// === format.test.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";

import { formatNumber } from "../../src/core/format.js";

/** The client hands i64 / u64 values over as bigint. */
describe("formatNumber", () => {
  it("prints a bigint in full, with the sign column of a number", () => {
    expect(formatNumber(12586269025n, { integer: true })).toBe(" 12586269025");
    expect(formatNumber(-42n)).toBe("-42");
    expect(formatNumber(0n)).toBe(" 0");
  });

  it("keeps every digit of a u64 beyond the doubles' exact range", () => {
    expect(formatNumber(18446744073709551615n, { integer: true })).toBe(" 18446744073709551615");
  });

  it("formats a number to six decimals, or as an integer", () => {
    expect(formatNumber(3, { integer: true })).toBe(" 3");
    expect(formatNumber(-1.5)).toBe("-1.500000");
  });
});
