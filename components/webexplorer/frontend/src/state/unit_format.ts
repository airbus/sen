// === unit_format.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// The C++ unit registry uses ASCII-only tokens (no /, sup-2, sup-3, deg, dot) for
// cross-language plumbing; we convert at the UI boundary. Mappings mirror
export function formatUnitAbbreviation(s: string | null | undefined): string {
  if (!s) return "";

  // Units whose ASCII form has no structural hint to transform from.
  const fullOverrides: Record<string, string> = {
    Nm: "N·m",
    kph: "km/h",
    deg: "°",
    degC: "°C",
    degF: "°F",
    arcmin: "′",
    arcsec: "″",
  };
  if (fullOverrides[s] !== undefined) return fullOverrides[s]!;

  let out = s;
  out = out.replace(/_per_/g, "/");
  out = out.replace(/_sq\b/g, "²");
  out = out.replace(/_cube\b/g, "³");
  out = out.replace(/3\b/g, "³");
  out = out.replace(/2\b/g, "²");
  // `deg` survives inside compounds after _per_ rewrite (e.g. deg_per_s -> deg/s).
  out = out.replace(/\bdeg\b/g, "°");
  return out;
}
