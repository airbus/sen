// === explorer_hosts.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { makeStore, useSelector } from "@sen/client/react";

import { selectionKey } from "../core/keys.js";
import { openPopout, type PopoutHandle } from "../ui/popout.js";
import type { Selection } from "./selection.js";

// Opening in-pane on an already-popped-out object focuses the popout instead; the
// live-selections list dedupes so a single popout isn't subscribed twice.

export interface OpenExplorer {
  selection: Selection;
  handle: PopoutHandle;
}

interface ExplorerHostsState {
  /** selectionKey -> { selection, popout handle }. */
  readonly openExplorers: ReadonlyMap<string, OpenExplorer>;
  /** Selection in the nav slot, or null when the nav slot shows the Interests tree. */
  readonly inPaneExplorer: Selection | null;
  /** Cached on state so `useSelector` returns a stable reference until membership changes. */
  readonly liveSelections: readonly Selection[];
}

function deriveLiveSelections(
  openExplorers: ReadonlyMap<string, OpenExplorer>,
  inPaneExplorer: Selection | null,
): readonly Selection[] {
  const out: Selection[] = [];
  for (const e of openExplorers.values()) out.push(e.selection);
  if (inPaneExplorer && !openExplorers.has(selectionKey(inPaneExplorer))) {
    out.push(inPaneExplorer);
  }
  return out;
}

const initialOpen: ReadonlyMap<string, OpenExplorer> = new Map();
const explorerStore = makeStore<ExplorerHostsState>({
  openExplorers: initialOpen,
  inPaneExplorer: null,
  liveSelections: deriveLiveSelections(initialOpen, null),
});

function toggleExplorer(sel: Selection): void {
  const key = selectionKey(sel);
  const existing = explorerStore.getState().openExplorers.get(key);
  if (existing) {
    try {
      existing.handle.popup.close();
    } catch {
      /* already closed */
    }
    explorerStore.setState((prev) => {
      const next = new Map(prev.openExplorers);
      next.delete(key);
      return {
        ...prev,
        openExplorers: next,
        liveSelections: deriveLiveSelections(next, prev.inPaneExplorer),
      };
    });
    return;
  }
  // Synchronous in the click handler so the browser sees the user gesture (popup blocker)
  // and document setup runs exactly once (StrictMode-safe).
  const handle = openPopout({ title: `${sel.objectName} - Sen Explorer` });
  if (!handle) {
    // eslint-disable-next-line no-console
    console.warn("Popup blocked: allow pop-ups for this origin to open the Object Explorer.");
    return;
  }
  explorerStore.setState((prev) => {
    const next = new Map(prev.openExplorers);
    next.set(key, { selection: sel, handle });
    return {
      ...prev,
      openExplorers: next,
      liveSelections: deriveLiveSelections(next, prev.inPaneExplorer),
    };
  });
}

// Focuses an existing popout or opens a new one; never closes a live popout.
// Wired to cmd-click / middle-click on object/property links.
function openExplorerInPopout(sel: Selection): void {
  const key = selectionKey(sel);
  const existing = explorerStore.getState().openExplorers.get(key);
  if (existing) {
    try {
      existing.handle.popup.focus();
      return;
    } catch {
      /* popup was closed out from under us; fall through and re-open */
    }
  }
  const handle = openPopout({ title: `${sel.objectName} - Sen Explorer` });
  if (!handle) {
    // eslint-disable-next-line no-console
    console.warn("Popup blocked: allow pop-ups for this origin to open the Object Explorer.");
    return;
  }
  explorerStore.setState((prev) => {
    const next = new Map(prev.openExplorers);
    next.set(key, { selection: sel, handle });
    return {
      ...prev,
      openExplorers: next,
      liveSelections: deriveLiveSelections(next, prev.inPaneExplorer),
    };
  });
}

function closeExplorerByKey(key: string): void {
  explorerStore.setState((prev) => {
    if (!prev.openExplorers.has(key)) return prev;
    const next = new Map(prev.openExplorers);
    next.delete(key);
    return {
      ...prev,
      openExplorers: next,
      liveSelections: deriveLiveSelections(next, prev.inPaneExplorer),
    };
  });
}

function openInPaneExplorer(sel: Selection): void {
  const existing = explorerStore.getState().openExplorers.get(selectionKey(sel));
  if (existing) {
    // Already popped out; focus that instead of stacking a redundant nav-slot view.
    try {
      existing.handle.popup.focus();
      return;
    } catch {
      /* popup was closed out from under us; fall through and open in-pane */
    }
  }
  explorerStore.setState((prev) => ({
    ...prev,
    inPaneExplorer: sel,
    liveSelections: deriveLiveSelections(prev.openExplorers, sel),
  }));
}

function closeInPaneExplorer(): void {
  explorerStore.setState((prev) =>
    prev.inPaneExplorer === null
      ? prev
      : {
          ...prev,
          inPaneExplorer: null,
          liveSelections: deriveLiveSelections(prev.openExplorers, null),
        },
  );
}

export const explorerHostsActions = Object.freeze({
  toggleExplorer,
  openExplorerInPopout,
  closeExplorerByKey,
  openInPaneExplorer,
  closeInPaneExplorer,
});

export function useOpenExplorers(): ReadonlyMap<string, OpenExplorer> {
  return useSelector(explorerStore, (s) => s.openExplorers);
}

export function useInPaneExplorer(): Selection | null {
  return useSelector(explorerStore, (s) => s.inPaneExplorer);
}

export function useLiveSelections(): readonly Selection[] {
  return useSelector(explorerStore, (s) => s.liveSelections);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    explorerStore.dispose();
  });
}
