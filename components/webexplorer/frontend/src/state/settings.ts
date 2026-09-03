// === settings.ts =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { makeStore, useSelector } from "@sen/client/react";

// User-tunable settings persisted to localStorage. Flat shape; adding a key is one
// field on Settings plus a default value.

const STORAGE_KEY = "webex.settings";

export interface Settings {
  /** Per-leaf history retained in SampleStore (drives plots). */
  sampleRetentionSeconds: number;
  /** Per-object history retained in EventStore (drives the events log). */
  eventRetentionSeconds: number;
}

export const DEFAULT_SETTINGS: Settings = {
  sampleRetentionSeconds: 60,
  eventRetentionSeconds: 60,
};

// 10 s avoids degenerate fast-trim; 600 s caps memory blast radius.
export const RETENTION_BOUNDS = { min: 10, max: 600 } as const;

function loadFromStorage(): Settings {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return DEFAULT_SETTINGS;
    const parsed = JSON.parse(raw) as unknown;
    if (typeof parsed !== "object" || parsed === null) return DEFAULT_SETTINGS;
    const r = parsed as Record<string, unknown>;
    return {
      sampleRetentionSeconds: clampRetention(r.sampleRetentionSeconds, DEFAULT_SETTINGS.sampleRetentionSeconds),
      eventRetentionSeconds: clampRetention(r.eventRetentionSeconds, DEFAULT_SETTINGS.eventRetentionSeconds),
    };
  } catch {
    return DEFAULT_SETTINGS;
  }
}

function clampRetention(v: unknown, fallback: number): number {
  if (typeof v !== "number" || !Number.isFinite(v)) return fallback;
  return Math.max(RETENTION_BOUNDS.min, Math.min(RETENTION_BOUNDS.max, Math.round(v)));
}

const settingsStore = makeStore<Settings>(loadFromStorage());

let lastPersisted = settingsStore.getState();
settingsStore.subscribe(() => {
  const current = settingsStore.getState();
  if (current === lastPersisted) return;
  lastPersisted = current;
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(current));
  } catch {
  }
});

function updateSettings(patch: Partial<Settings>): void {
  settingsStore.setState((prev) => {
    let changed = false;
    for (const key of Object.keys(patch) as (keyof Settings)[]) {
      if (patch[key] !== prev[key]) {
        changed = true;
        break;
      }
    }
    if (!changed) return prev;
    return { ...prev, ...patch };
  });
}

export const settingsActions = Object.freeze({ update: updateSettings });

export function useSampleRetentionSeconds(): number {
  return useSelector(settingsStore, (s) => s.sampleRetentionSeconds);
}

export function useEventRetentionSeconds(): number {
  return useSelector(settingsStore, (s) => s.eventRetentionSeconds);
}

/** Prefer the per-field hooks when you only need one knob. */
export function useSettings(): Settings {
  return useSelector(settingsStore, (s) => s);
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    settingsStore.dispose();
  });
}
