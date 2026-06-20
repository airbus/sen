// === parse_sen_timestamp.test.ts =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { parseSenTimestamp } from "../src/handles.js";

// Fixture mirrors the wire shape emitted by the C++ side's
// libs/core/src/base/timestamp.cpp::toUtcStringNs (RFC-3339, UTC, 9-digit nanoseconds).
// The cross-component contract is the "Wire types" table in components/jsonrpc/architecture.md.
describe("parseSenTimestamp", () => {
  it("parses the canonical RFC-3339 ns wire format", () => {
    const parsed = parseSenTimestamp("2026-06-19T07:40:12.345678901Z");
    expect(parsed).not.toBeNull();
    expect(parsed!.date.toISOString()).toBe("2026-06-19T07:40:12.345Z");
    expect(parsed!.nanoseconds).toBe(678901);
  });

  it("preserves zero-padded sub-millisecond fractions", () => {
    const parsed = parseSenTimestamp("2026-01-01T00:00:00.000000042Z");
    expect(parsed).not.toBeNull();
    expect(parsed!.date.getTime()).toBe(Date.UTC(2026, 0, 1, 0, 0, 0, 0));
    expect(parsed!.nanoseconds).toBe(42);
  });

  it("rejects the legacy space-separated microsecond format", () => {
    expect(parseSenTimestamp("2026-06-19 07:40:12 345678")).toBeNull();
  });

  it("rejects fractional widths other than 9 digits", () => {
    expect(parseSenTimestamp("2026-06-19T07:40:12.345Z")).toBeNull();
    expect(parseSenTimestamp("2026-06-19T07:40:12.3456789012Z")).toBeNull();
  });

  it("rejects missing or wrong suffix", () => {
    expect(parseSenTimestamp("2026-06-19T07:40:12.345678901")).toBeNull();
    expect(parseSenTimestamp("2026-06-19T07:40:12.345678901+00:00")).toBeNull();
  });

  it("trims surrounding whitespace before matching", () => {
    const parsed = parseSenTimestamp("  2026-06-19T07:40:12.000000000Z  ");
    expect(parsed).not.toBeNull();
    expect(parsed!.nanoseconds).toBe(0);
  });
});
