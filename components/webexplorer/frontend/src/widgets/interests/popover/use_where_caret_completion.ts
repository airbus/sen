// === use_where_caret_completion.ts ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useMemo, useState } from "react";
import type * as React from "react";

import { wordUnderCaret } from "./sql_helpers.js";

export interface WhereCaretCompletion {
  syncCaret: () => void;
  currentWord: { word: string; start: number; end: number };
  replaceWordWithToken: (token: string) => void;
}

export function useWhereCaretCompletion(
  whereClause: string,
  setWhereClause: (value: string) => void,
  inputRef: React.MutableRefObject<HTMLInputElement | null>,
): WhereCaretCompletion {
  const [caret, setCaret] = useState(0);

  const syncCaret = useCallback(() => {
    const el = inputRef.current;
    if (el) setCaret(el.selectionStart ?? el.value.length);
  }, [inputRef]);

  const currentWord = useMemo(() => wordUnderCaret(whereClause, caret), [whereClause, caret]);

  const replaceWordWithToken = useCallback(
    (token: string) => {
      const before = whereClause.slice(0, currentWord.start);
      const after = whereClause.slice(currentWord.end);
      const needsLead = before.length > 0 && !/\s$/.test(before);
      const needsTrail = !after.startsWith(" ");
      const insert = `${needsLead ? " " : ""}${token}${needsTrail ? " " : ""}`;
      const next = `${before}${insert}${after}`;
      const newCaret = currentWord.start + insert.length;
      setWhereClause(next);
      setCaret(newCaret);
      requestAnimationFrame(() => {
        const el = inputRef.current;
        if (!el) return;
        el.focus();
        el.setSelectionRange(newCaret, newCaret);
      });
    },
    [whereClause, currentWord.start, currentWord.end, setWhereClause, inputRef],
  );

  return { syncCaret, currentWord, replaceWordWithToken };
}
