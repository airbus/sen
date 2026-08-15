// === keys.ts =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// `\0`-delimited composite keys; one canonical encoding so dedup/membership checks
// can't silently disagree across call sites.

/** `${session}\0${bus}`. */
export function busKey(session: string, bus: string): string {
  return `${session}\0${bus}`;
}

export function parseBusKey(key: string): { sessionName: string; busName: string } | null {
  const i = key.indexOf("\0");
  if (i < 0) return null;
  return { sessionName: key.slice(0, i), busName: key.slice(i + 1) };
}

/** `${session}\0${bus}\0${queryName}`. */
export function queryKey(session: string, bus: string, queryName: string): string {
  return `${session}\0${bus}\0${queryName}`;
}

/** `${session}\0${bus}\0${objectName}`. Pin identity is interest-independent. */
export function pinKey(session: string, bus: string, objectName: string): string {
  return `${session}\0${bus}\0${objectName}`;
}

// Structural input on purpose so `core/` doesn't depend on `state/`.
export function selectionKey(sel: {
  sessionName: string;
  busName: string;
  objectName: string;
}): string {
  return pinKey(sel.sessionName, sel.busName, sel.objectName);
}

/** `${interestName}\0${objectName}`. */
export function objectEventKey(interestName: string, objectName: string): string {
  return `${interestName}\0${objectName}`;
}

/** The `(session, bus)` prefix of a pinKey for membership checks against busKey sets. */
export function pinKeyBusPrefix(key: string): string | null {
  const first = key.indexOf("\0");
  if (first < 0) return null;
  const second = key.indexOf("\0", first + 1);
  if (second < 0) return null;
  return key.slice(0, second);
}

export function parsePinKey(
  key: string,
): { sessionName: string; busName: string; objectName: string } | null {
  const first = key.indexOf("\0");
  if (first < 0) return null;
  const second = key.indexOf("\0", first + 1);
  if (second < 0) return null;
  return {
    sessionName: key.slice(0, first),
    busName: key.slice(first + 1, second),
    objectName: key.slice(second + 1),
  };
}
