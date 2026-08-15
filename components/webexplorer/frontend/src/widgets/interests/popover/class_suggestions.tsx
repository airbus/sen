// === ClassSuggestions.tsx ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo } from "react";

import type { Client } from "@sen/client";

import { useKnownClassNames } from "./sql_helpers.js";
import { Chip, ChipRow, Hintette } from "./where_guidance.js";

export function useClassMatches(client: Client | null, value: string): {
  matches: string[];
  knownCount: number;
} {
  const known = useKnownClassNames(client);
  const matches = useMemo(() => {
    const q = value.trim().toLowerCase();
    if (q === "" || q === "*") return known.slice(0, 8);
    return known.filter((c) => c.toLowerCase().includes(q)).slice(0, 8);
  }, [known, value]);
  return { matches, knownCount: known.length };
}

export function ClassSuggestions({
  matches,
  knownCount,
  focusedIdx,
  onPick,
}: {
  matches: string[];
  knownCount: number;
  focusedIdx: number;
  onPick: (name: string) => void;
}) {
  if (matches.length === 0) {
    if (knownCount === 0) return null;
    return (
      <ChipRow>
        <Hintette>no class matches; any class name is typeable.</Hintette>
      </ChipRow>
    );
  }
  return (
    <>
      <ChipRow>
        <Hintette>
          Suggested classes (any class name is typeable, including ones not yet observed):
        </Hintette>
      </ChipRow>
      <ChipRow>
        {matches.map((c, i) => (
          <Chip
            key={c}
            onClick={() => onPick(c)}
            title={`Use ${c}`}
            focused={i === focusedIdx}
          >
            {c}
          </Chip>
        ))}
      </ChipRow>
    </>
  );
}
