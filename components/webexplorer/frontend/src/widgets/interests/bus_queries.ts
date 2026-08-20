// === bus_queries.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

export type OrderBy = "name" | "class";

export interface NamedQuery {
  name: string;
  query: string;
}

export function interestNameFor(session: string, bus: string, queryName: string): string {
  return `${session}.${bus}.${queryName}`;
}

// Session/bus names are dot-free upstream, so the two leading dots split unambiguously.
export function parseInterestName(
  interestName: string,
): { sessionName: string; busName: string; queryName: string } | null {
  const firstDot = interestName.indexOf(".");
  if (firstDot < 0) return null;
  const secondDot = interestName.indexOf(".", firstDot + 1);
  if (secondDot < 0) return null;
  return {
    sessionName: interestName.slice(0, firstDot),
    busName: interestName.slice(firstDot + 1, secondDot),
    queryName: interestName.slice(secondDot + 1),
  };
}

export const DEFAULT_QUERY_NAME = "all";

export function defaultQueryBodyFor(session: string, bus: string): string {
  return `SELECT * FROM ${session}.${bus}`;
}

const BUS_QUERIES_KEY = "webex.nav.busQueries";

export function loadBusQueries(): Record<string, NamedQuery[]> {
  try {
    const raw = localStorage.getItem(BUS_QUERIES_KEY);
    if (!raw) return {};
    const parsed = JSON.parse(raw);
    if (typeof parsed !== "object" || parsed === null) return {};
    const out: Record<string, NamedQuery[]> = {};
    for (const [k, v] of Object.entries(parsed)) {
      if (!Array.isArray(v)) continue;
      const list: NamedQuery[] = [];
      for (const item of v) {
        if (item && typeof item.name === "string" && typeof item.query === "string") {
          list.push({ name: item.name, query: item.query });
        }
      }
      out[k] = list;
    }
    return out;
  } catch {
    return {};
  }
}

export function saveBusQueries(queries: Record<string, NamedQuery[]>): void {
  try {
    localStorage.setItem(BUS_QUERIES_KEY, JSON.stringify(queries));
  } catch {
  }
}
