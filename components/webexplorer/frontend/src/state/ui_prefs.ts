// === ui_prefs.ts =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { makeStore, useSelector } from "@sen/client/react";

import { pinKeyBusPrefix } from "../core/keys.js";

// Per-pane / per-pin UI preferences. Per-field selector hooks keep one slot's update
// from re-rendering siblings during a drag.

function readLocalStorageInt(key: string, fallback: number, min: number): number {
  try {
    const raw = localStorage.getItem(key);
    const n = raw ? parseInt(raw, 10) : NaN;
    return Number.isFinite(n) && n >= min ? n : fallback;
  } catch {
    return fallback;
  }
}

function writeLocalStorage(key: string, value: string): void {
  try {
    localStorage.setItem(key, value);
  } catch {
  }
}

// --- Bottom pane (Plots drawer) ---

interface BottomPaneState {
  folded: boolean;
  height: number;
}

const BOTTOM_HEIGHT_KEY = "webex.bottomHeight";

const bottomPaneStore = makeStore<BottomPaneState>({
  // Not persisted; App's panel-count effect auto-unfolds on the first plot.
  folded: true,
  height: readLocalStorageInt(BOTTOM_HEIGHT_KEY, 300, 120),
});

// Height persists on drag-end commit, not on every live tick.
function setBottomFolded(updater: boolean | ((prev: boolean) => boolean)): void {
  bottomPaneStore.setState((prev) => {
    const folded = typeof updater === "function" ? updater(prev.folded) : updater;
    if (folded === prev.folded) return prev;
    return { ...prev, folded };
  });
}

function setBottomHeight(height: number): void {
  bottomPaneStore.setState((prev) => (prev.height === height ? prev : { ...prev, height }));
}

function commitBottomHeight(height: number): void {
  setBottomHeight(height);
  writeLocalStorage(BOTTOM_HEIGHT_KEY, String(height));
}

export const bottomPaneActions = Object.freeze({
  setFolded: setBottomFolded,
  setHeight: setBottomHeight,
  commitHeight: commitBottomHeight,
});

export function useBottomFolded(): boolean {
  return useSelector(bottomPaneStore, (s) => s.folded);
}

export function useBottomHeight(): number {
  return useSelector(bottomPaneStore, (s) => s.height);
}

// --- Bottom pane active tab ---

const BOTTOM_TAB_KEY = "webex.bottomTab";

function readBottomTab(): string {
  try {
    return localStorage.getItem(BOTTOM_TAB_KEY) ?? "plots";
  } catch {
    return "plots";
  }
}

const bottomTabStore = makeStore<{ key: string }>({ key: readBottomTab() });

let lastPersistedBottomTab = bottomTabStore.getState().key;
bottomTabStore.subscribe(() => {
  const { key } = bottomTabStore.getState();
  if (key === lastPersistedBottomTab) return;
  lastPersistedBottomTab = key;
  writeLocalStorage(BOTTOM_TAB_KEY, key);
});

function setBottomTab(key: string): void {
  bottomTabStore.setState((prev) => (prev.key === key ? prev : { key }));
}

export const bottomTabActions = Object.freeze({ set: setBottomTab });

export function useBottomTab(): string {
  return useSelector(bottomTabStore, (s) => s.key);
}

// --- Workspace pane visibility ---

interface WorkspacePaneState {
  hidden: boolean;
  animating: boolean;
}

const WS_HIDDEN_KEY = "webex.wsHidden";

const workspacePaneStore = makeStore<WorkspacePaneState>({
  hidden: (() => {
    try {
      return localStorage.getItem(WS_HIDDEN_KEY) === "1";
    } catch {
      return false;
    }
  })(),
  animating: false,
});

let lastPersistedHidden = workspacePaneStore.getState().hidden;
workspacePaneStore.subscribe(() => {
  const { hidden } = workspacePaneStore.getState();
  if (hidden === lastPersistedHidden) return;
  lastPersistedHidden = hidden;
  writeLocalStorage(WS_HIDDEN_KEY, hidden ? "1" : "0");
});

let animatingTimeout: ReturnType<typeof setTimeout> | null = null;

function toggleWorkspace(): void {
  if (animatingTimeout !== null) clearTimeout(animatingTimeout);
  workspacePaneStore.setState((prev) => ({ hidden: !prev.hidden, animating: true }));
  // 240ms > 220ms CSS transition so the flag is still true when the animation completes.
  animatingTimeout = setTimeout(() => {
    animatingTimeout = null;
    workspacePaneStore.setState((prev) =>
      prev.animating ? { ...prev, animating: false } : prev,
    );
  }, 240);
}

export const workspacePaneActions = Object.freeze({
  toggle: toggleWorkspace,
});

export function useWorkspaceHidden(): boolean {
  return useSelector(workspacePaneStore, (s) => s.hidden);
}

export function useWorkspaceAnimating(): boolean {
  return useSelector(workspacePaneStore, (s) => s.animating);
}

// --- Pinned objects (Overview workspace input) ---
//
// Each pin carries the captured className so the Overview can group by class without
// walking open interests per render; className persists across the object going offline.

export interface PinnedObject {
  className: string;
}

interface PinnedObjectsState {
  pinned: ReadonlyMap<string, PinnedObject>;
}

const PINNED_KEY = "webex.overview.pinned";

const pinnedObjectsStore = makeStore<PinnedObjectsState>({
  pinned: readPinned(),
});

function readPinned(): ReadonlyMap<string, PinnedObject> {
  try {
    const raw = localStorage.getItem(PINNED_KEY);
    if (!raw) return new Map();
    const parsed = JSON.parse(raw) as unknown;
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) return new Map();
    const out = new Map<string, PinnedObject>();
    for (const [k, v] of Object.entries(parsed as Record<string, unknown>)) {
      if (typeof k !== "string") continue;
      if (!v || typeof v !== "object") continue;
      const className = (v as { className?: unknown }).className;
      if (typeof className !== "string" || className.length === 0) continue;
      out.set(k, { className });
    }
    return out;
  } catch {
    return new Map();
  }
}

let lastPersistedPinned = pinnedObjectsStore.getState().pinned;
pinnedObjectsStore.subscribe(() => {
  const { pinned } = pinnedObjectsStore.getState();
  if (pinned === lastPersistedPinned) return;
  lastPersistedPinned = pinned;
  const obj: Record<string, PinnedObject> = {};
  for (const [k, v] of pinned) obj[k] = v;
  writeLocalStorage(PINNED_KEY, JSON.stringify(obj));
});

function togglePinned(pinKey: string, className: string): void {
  // Read directly so layout cleanup doesn't depend on the updater running exactly once
  // (StrictMode double-invoke).
  const wasPinned = pinnedObjectsStore.getState().pinned.has(pinKey);
  pinnedObjectsStore.setState((prev) => {
    const next = new Map(prev.pinned);
    if (next.has(pinKey)) next.delete(pinKey);
    else next.set(pinKey, { className });
    return { pinned: next };
  });
  // Drop saved layout so unpin + re-pin doesn't resurrect old (x,y,w,h).
  if (wasPinned) removeCockpitCardLayout(pinKey);
}

// Objects that come and go within a still-present bus are NOT pruned (user explicitly pinned).
function prunePinnedObjectsByBusKeys(validBusKeyPrefixes: ReadonlySet<string>): void {
  const droppedKeys: string[] = [];
  pinnedObjectsStore.setState((prev) => {
    const next = new Map<string, PinnedObject>();
    for (const [k, v] of prev.pinned) {
      const busPrefix = pinKeyBusPrefix(k);
      if (busPrefix === null || validBusKeyPrefixes.has(busPrefix)) next.set(k, v);
      else droppedKeys.push(k);
    }
    return droppedKeys.length === 0 ? prev : { pinned: next };
  });
  for (const k of droppedKeys) removeCockpitCardLayout(k);
}

function clearAllPinnedObjects(): void {
  pinnedObjectsStore.setState((prev) =>
    prev.pinned.size === 0 ? prev : { pinned: new Map() },
  );
  clearAllCockpitCardLayouts();
}

export const pinnedObjectsActions = Object.freeze({
  toggle: togglePinned,
  pruneByBusKeys: prunePinnedObjectsByBusKeys,
  clearAll: clearAllPinnedObjects,
});

export function usePinnedObjects(): ReadonlyMap<string, PinnedObject> {
  return useSelector(pinnedObjectsStore, (s) => s.pinned);
}

// --- Overview workspace density mode ---
//   cockpit: ~260x200, 4 properties, full chrome (default).
//   tile:    ~180x120, 2 properties, flatter chrome.
// Drives CARD_WIDTH / CARD_HEIGHT and MAX_CARD_PROPS_BY_DENSITY.

export type OverviewDensityMode = "cockpit" | "tile";

const OVERVIEW_DENSITY_KEY = "webex.overview.densityMode";

function readOverviewDensityMode(): OverviewDensityMode {
  try {
    return localStorage.getItem(OVERVIEW_DENSITY_KEY) === "tile" ? "tile" : "cockpit";
  } catch {
    return "cockpit";
  }
}

const overviewDensityStore = makeStore<{ density: OverviewDensityMode }>({
  density: readOverviewDensityMode(),
});

let lastPersistedOverviewDensity = overviewDensityStore.getState().density;
overviewDensityStore.subscribe(() => {
  const { density } = overviewDensityStore.getState();
  if (density === lastPersistedOverviewDensity) return;
  lastPersistedOverviewDensity = density;
  writeLocalStorage(OVERVIEW_DENSITY_KEY, density);
});

function setOverviewDensityMode(density: OverviewDensityMode): void {
  overviewDensityStore.setState((prev) => (prev.density === density ? prev : { density }));
}

export const overviewDensityActions = Object.freeze({ set: setOverviewDensityMode });

export function useOverviewDensityMode(): OverviewDensityMode {
  return useSelector(overviewDensityStore, (s) => s.density);
}


// --- Cockpit per-card free-form layouts ---
// Cockpit density allows free-form drag/resize; layout persists per pinKey. Tile mode
// ignores this store (uniform sizing + auto-pack).

export interface CockpitCardLayout {
  readonly x: number;
  readonly y: number;
  readonly w: number;
  readonly h: number;
}

// V2 schema = 130px column unit; the V2 suffix isolates from any leftover V1 entries.
const COCKPIT_LAYOUTS_KEY = "webex.overview.cockpitLayoutsV2";

function readCockpitCardLayouts(): ReadonlyMap<string, CockpitCardLayout> {
  try {
    const raw = localStorage.getItem(COCKPIT_LAYOUTS_KEY);
    if (!raw) return new Map();
    const parsed = JSON.parse(raw);
    if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) return new Map();
    const out = new Map<string, CockpitCardLayout>();
    for (const [k, v] of Object.entries(parsed)) {
      if (typeof k !== "string" || !v || typeof v !== "object") continue;
      const r = v as Partial<CockpitCardLayout>;
      if (
        typeof r.x === "number" &&
        Number.isFinite(r.x) &&
        typeof r.y === "number" &&
        Number.isFinite(r.y) &&
        typeof r.w === "number" &&
        Number.isFinite(r.w) &&
        r.w >= 1 &&
        typeof r.h === "number" &&
        Number.isFinite(r.h) &&
        r.h >= 1
      ) {
        out.set(k, { x: Math.max(0, r.x), y: Math.max(0, r.y), w: r.w, h: r.h });
      }
    }
    return out;
  } catch {
    return new Map();
  }
}

const cockpitCardLayoutsStore = makeStore<{
  layouts: ReadonlyMap<string, CockpitCardLayout>;
}>({ layouts: readCockpitCardLayouts() });

let lastPersistedCockpitLayouts = cockpitCardLayoutsStore.getState().layouts;
cockpitCardLayoutsStore.subscribe(() => {
  const { layouts } = cockpitCardLayoutsStore.getState();
  if (layouts === lastPersistedCockpitLayouts) return;
  lastPersistedCockpitLayouts = layouts;
  const obj: Record<string, CockpitCardLayout> = {};
  for (const [k, v] of layouts) obj[k] = v;
  writeLocalStorage(COCKPIT_LAYOUTS_KEY, JSON.stringify(obj));
});

function setCockpitCardLayouts(entries: ReadonlyArray<{ pinKey: string; layout: CockpitCardLayout }>): void {
  cockpitCardLayoutsStore.setState((prev) => {
    let changed = false;
    const next = new Map(prev.layouts);
    for (const { pinKey, layout } of entries) {
      const cur = next.get(pinKey);
      if (
        !cur ||
        cur.x !== layout.x ||
        cur.y !== layout.y ||
        cur.w !== layout.w ||
        cur.h !== layout.h
      ) {
        next.set(pinKey, layout);
        changed = true;
      }
    }
    return changed ? { layouts: next } : prev;
  });
}

function removeCockpitCardLayout(pinKey: string): void {
  cockpitCardLayoutsStore.setState((prev) => {
    if (!prev.layouts.has(pinKey)) return prev;
    const next = new Map(prev.layouts);
    next.delete(pinKey);
    return { layouts: next };
  });
}

function clearAllCockpitCardLayouts(): void {
  cockpitCardLayoutsStore.setState((prev) =>
    prev.layouts.size === 0 ? prev : { layouts: new Map() },
  );
}

export const cockpitCardLayoutsActions = Object.freeze({
  setMany: setCockpitCardLayouts,
  remove: removeCockpitCardLayout,
  clearAll: clearAllCockpitCardLayouts,
});

export function useCockpitCardLayouts(): ReadonlyMap<string, CockpitCardLayout> {
  return useSelector(cockpitCardLayoutsStore, (s) => s.layouts);
}

// HMR: dispose stores so iterations don't accumulate orphan persistence subscribers.
if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    if (animatingTimeout !== null) {
      clearTimeout(animatingTimeout);
      animatingTimeout = null;
    }
    bottomPaneStore.dispose();
    workspacePaneStore.dispose();
    bottomTabStore.dispose();
    pinnedObjectsStore.dispose();
    overviewDensityStore.dispose();
    cockpitCardLayoutsStore.dispose();
  });
}
