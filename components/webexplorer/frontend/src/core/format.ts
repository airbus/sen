// === format.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { parseSenTimestamp } from "@sen/client";

// Leading U+00A0 on non-negatives keeps the column aligned when values flip sign;
// a regular space would collapse inside the span.
export function formatNumber(v: number, opts: { integer?: boolean } = {}): string {
  if (!isFinite(v)) return String(v);
  const body = opts.integer ? Math.trunc(v).toString() : v.toFixed(6);
  return body.startsWith("-") ? body : " " + body;
}

/** `HH:MM:SS.mmmuuu` in UTC (matches the wire), returns the wire string unchanged on
 *  parse failure. UTC accessors match what cell tooltips advertise ("UTC (server)") and
 *  keep timestamps stable across users viewing the same recorded session from different
 *  timezones. */
export function formatTimestamp(wire: string): string {
  const parsed = parseSenTimestamp(wire);
  if (!parsed) return wire;
  const d = parsed.date;
  const hh = String(d.getUTCHours()).padStart(2, "0");
  const mm = String(d.getUTCMinutes()).padStart(2, "0");
  const ss = String(d.getUTCSeconds()).padStart(2, "0");
  const ms = String(d.getUTCMilliseconds()).padStart(3, "0");
  // `nanoseconds` is the sub-millisecond remainder (0..999999); keep the microsecond triplet.
  const us = String(Math.floor(parsed.nanoseconds / 1000)).padStart(3, "0");
  return `${hh}:${mm}:${ss}.${ms}${us}`;
}

/** Last dotted segment, e.g. `aircrafts.DummyAircraft` -> `DummyAircraft`. */
export function shortName(qualified: string): string {
  const ix = qualified.lastIndexOf(".");
  return ix >= 0 ? qualified.slice(ix + 1) : qualified;
}

// Adaptive width so live-updating labels don't reflow column widths.
export function formatValue(v: number): string {
  const a = Math.abs(v);
  if (a >= 1000) return v.toFixed(0);
  if (a >= 100) return v.toFixed(1);
  if (a >= 10) return v.toFixed(2);
  return v.toFixed(3);
}
