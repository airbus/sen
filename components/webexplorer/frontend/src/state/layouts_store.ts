// === layouts_store.ts ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Named layouts persisted under `webex.layouts`. Each captures a LayoutSnapshot plus
// name + savedAt. The `webex.layouts` key itself is excluded from snapshots so the
// list persists across layout switches.

import { makeStore, useSelector } from "@sen/client/react";

import { captureSnapshot, type LayoutSnapshot } from "./layout_snapshot.js";

export interface NamedLayout {
  name: string;
  savedAt: number;
  snapshot: LayoutSnapshot;
}

interface LayoutsState {
  layouts: ReadonlyMap<string, NamedLayout>;
}

const LAYOUTS_KEY = "webex.layouts";
const SCHEMA_VERSION = 1;

interface PersistedLayoutsV1 {
  version: typeof SCHEMA_VERSION;
  layouts: Array<{ name: string; savedAt: number; snapshot: Record<string, string> }>;
}

function readLayouts(): ReadonlyMap<string, NamedLayout> {
  try {
    const raw = localStorage.getItem(LAYOUTS_KEY);
    if (!raw) return new Map();
    const parsed = JSON.parse(raw) as Partial<PersistedLayoutsV1>;
    if (!parsed || parsed.version !== SCHEMA_VERSION || !Array.isArray(parsed.layouts)) {
      return new Map();
    }
    const out = new Map<string, NamedLayout>();
    for (const entry of parsed.layouts) {
      if (
        !entry ||
        typeof entry.name !== "string" ||
        typeof entry.savedAt !== "number" ||
        !entry.snapshot ||
        typeof entry.snapshot !== "object"
      ) {
        continue;
      }
      // Snapshot must be string->string.
      const snap: Record<string, string> = {};
      for (const [k, v] of Object.entries(entry.snapshot)) {
        if (typeof k === "string" && typeof v === "string") snap[k] = v;
      }
      out.set(entry.name, { name: entry.name, savedAt: entry.savedAt, snapshot: snap });
    }
    return out;
  } catch {
    return new Map();
  }
}

function writeLayouts(layouts: ReadonlyMap<string, NamedLayout>): void {
  try {
    const payload: PersistedLayoutsV1 = {
      version: SCHEMA_VERSION,
      layouts: Array.from(layouts.values()).map((l) => ({
        name: l.name,
        savedAt: l.savedAt,
        snapshot: l.snapshot as Record<string, string>,
      })),
    };
    localStorage.setItem(LAYOUTS_KEY, JSON.stringify(payload));
  } catch {
  }
}

const layoutsStore = makeStore<LayoutsState>({ layouts: readLayouts() });

let lastPersisted = layoutsStore.getState().layouts;
layoutsStore.subscribe(() => {
  const { layouts } = layoutsStore.getState();
  if (layouts === lastPersisted) return;
  lastPersisted = layouts;
  writeLayouts(layouts);
});

/** Overwrites any existing layout with the same name. */
function saveCurrentAs(name: string): void {
  if (!name) return;
  const snapshot = captureSnapshot();
  layoutsStore.setState((prev) => {
    const next = new Map(prev.layouts);
    next.set(name, { name, savedAt: Date.now(), snapshot });
    return { layouts: next };
  });
}

function upsert(layout: NamedLayout): void {
  if (!layout.name) return;
  layoutsStore.setState((prev) => {
    const next = new Map(prev.layouts);
    next.set(layout.name, layout);
    return { layouts: next };
  });
}

function remove(name: string): void {
  layoutsStore.setState((prev) => {
    if (!prev.layouts.has(name)) return prev;
    const next = new Map(prev.layouts);
    next.delete(name);
    return { layouts: next };
  });
}

function rename(oldName: string, newName: string): void {
  if (!newName || oldName === newName) return;
  layoutsStore.setState((prev) => {
    const existing = prev.layouts.get(oldName);
    if (!existing) return prev;
    const next = new Map(prev.layouts);
    next.delete(oldName);
    next.set(newName, { ...existing, name: newName });
    return { layouts: next };
  });
}

export const layoutsActions = Object.freeze({
  saveCurrentAs,
  upsert,
  remove,
  rename,
});

export function useLayouts(): ReadonlyMap<string, NamedLayout> {
  return useSelector(layoutsStore, (s) => s.layouts);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    layoutsStore.dispose();
  });
}
