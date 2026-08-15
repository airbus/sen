// === sql_helpers.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo } from "react";

import type { Client, CustomTypeSpec } from "@sen/client";

export const SQL_COMPARE_OPS = ["=", ">", "<", ">=", "<=", "BETWEEN", "IN"] as const;
export const SQL_LOGICAL_OPS = ["AND", "OR", "NOT"] as const;
export const SQL_PSEUDO_PROPS = ["name", "id"] as const;

// Word chars: [A-Za-z0-9_.] - the dot allows qualified paths like spatial.SpatialFPStruct.lon.
export function wordUnderCaret(
  text: string,
  caret: number,
): { word: string; start: number; end: number } {
  const isWord = (c: string | undefined) => c !== undefined && /[A-Za-z0-9_.]/.test(c);
  let start = caret;
  while (start > 0 && isWord(text[start - 1])) start--;
  let end = caret;
  while (end < text.length && isWord(text[end])) end++;
  return { word: text.slice(start, end), start, end };
}

export function useKnownClassNames(client: Client | null): string[] {
  return useMemo(() => {
    if (!client) return [];
    return client
      .listKnownTypes()
      .filter((s) => s.data.type === "sen.kernel.ClassTypeSpec")
      .map((s) => s.qualifiedName)
      .sort((a, b) => a.localeCompare(b));
  }, [client]);
}

// Returns null when the class spec isn't cached yet (caller falls back to operators only).
export function useClassProperties(client: Client | null, className: string): string[] | null {
  return useMemo(() => {
    if (!client || !className || className === "*") return null;
    const seen = new Set<string>();
    const out: string[] = [];
    const visit = (name: string): boolean => {
      if (seen.has(name)) return true;
      seen.add(name);
      const spec = client.getType(name) as CustomTypeSpec | undefined;
      if (!spec || spec.data.type !== "sen.kernel.ClassTypeSpec") return false;
      for (const p of spec.data.value.properties) {
        if (!out.includes(p.name)) out.push(p.name);
      }
      for (const parent of spec.data.value.parents) visit(parent);
      return true;
    };
    return visit(className) ? out : null;
  }, [client, className]);
}
