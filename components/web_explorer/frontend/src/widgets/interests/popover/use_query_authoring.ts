// === use_query_authoring.ts ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useMemo, useState } from "react";

import { DEFAULT_QUERY_NAME } from "../bus_queries.js";
import type { AuthoringMode } from "./mode_toggle.js";

export interface QueryAuthoringState {
  name: string;
  setName: (value: string) => void;
  mode: AuthoringMode;
  switchMode: (next: AuthoringMode) => void;
  classSpec: string;
  setClassSpec: (value: string) => void;
  whereClause: string;
  setWhereClause: (value: string) => void;
  rawSql: string;
  setRawSql: (value: string) => void;
  composedQuery: string;
}

function composeGuided(sessionName: string, busName: string, cls: string, where: string): string {
  const cleanCls = cls.trim() || "*";
  const cleanWhere = where.trim();
  const head = `SELECT ${cleanCls} FROM ${sessionName}.${busName}`;
  return cleanWhere ? `${head} WHERE ${cleanWhere}` : head;
}

// Returns null when the raw SQL doesn't match `SELECT ... FROM <session>.<bus> [WHERE ...]`;
// caller keeps guided fields as-is in that case.
function decomposeToGuided(
  sessionName: string,
  busName: string,
  raw: string,
): { cls: string; where: string } | null {
  const pattern = new RegExp(
    String.raw`^\s*SELECT\s+(.+?)\s+FROM\s+${escapeRegex(sessionName)}\.${escapeRegex(busName)}` +
      String.raw`(?:\s+WHERE\s+(.+?))?\s*;?\s*$`,
    "is",
  );
  const match = pattern.exec(raw);
  if (!match) return null;
  const cls = match[1]!.trim();
  const where = (match[2] ?? "").trim();
  return { cls, where };
}

function escapeRegex(s: string): string {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

export function useQueryAuthoring({
  sessionName,
  busName,
  existingNames,
}: {
  sessionName: string;
  busName: string;
  existingNames: string[];
}): QueryAuthoringState {
  // Pre-fill "all" only when unused, so default-named recreate works without name collisions.
  const [name, setName] = useState(
    existingNames.includes(DEFAULT_QUERY_NAME) ? "" : DEFAULT_QUERY_NAME,
  );
  const [mode, setMode] = useState<AuthoringMode>("guided");
  const [classSpec, setClassSpec] = useState("*");
  const [whereClause, setWhereClause] = useState("");
  const [rawSql, setRawSql] = useState(`SELECT * FROM ${sessionName}.${busName}`);

  // Mode transitions seed each other so the flip is reversible without losing edits.
  const switchMode = useCallback(
    (next: AuthoringMode) => {
      if (next === "raw" && mode === "guided") {
        setRawSql(composeGuided(sessionName, busName, classSpec, whereClause));
      } else if (next === "guided" && mode === "raw") {
        const parsed = decomposeToGuided(sessionName, busName, rawSql);
        if (parsed) {
          setClassSpec(parsed.cls);
          setWhereClause(parsed.where);
        }
      }
      setMode(next);
    },
    [mode, classSpec, whereClause, sessionName, busName, rawSql],
  );

  const composedQuery = useMemo(
    () => (mode === "raw" ? rawSql.trim() : composeGuided(sessionName, busName, classSpec, whereClause)),
    [mode, rawSql, sessionName, busName, classSpec, whereClause],
  );

  return {
    name,
    setName,
    mode,
    switchMode,
    classSpec,
    setClassSpec,
    whereClause,
    setWhereClause,
    rawSql,
    setRawSql,
    composedQuery,
  };
}
