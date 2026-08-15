// === layout_snapshot.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// A named layout is a copy of every `webex.*` localStorage key. Applying one rewrites
// the keys and reloads so every module-scoped store re-initialises from them; this
// piggybacks on existing init paths and picks up future stores for free.

const KEY_PREFIX = "webex.";

// `webex.layouts` is excluded so layouts don't contain each other; retention values
// stay with the user across layout switches.
const EXCLUDE = new Set([
  "webex.layouts",
  "webex.sampleRetentionSeconds",
  "webex.eventRetentionSeconds",
]);

export type LayoutSnapshot = Readonly<Record<string, string>>;

export function captureSnapshot(): LayoutSnapshot {
  const out: Record<string, string> = {};
  try {
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i);
      if (key === null) continue;
      if (!key.startsWith(KEY_PREFIX)) continue;
      if (EXCLUDE.has(key)) continue;
      const value = localStorage.getItem(key);
      if (value !== null) out[key] = value;
    }
  } catch {
  }
  return out;
}

// Keys missing from the snapshot are cleared so a layout switch doesn't leak state
// from the previous layout.
export function applySnapshot(snapshot: LayoutSnapshot): void {
  try {
    // Collect first; we can't mutate localStorage while iterating.
    const toRemove: string[] = [];
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i);
      if (key === null) continue;
      if (!key.startsWith(KEY_PREFIX)) continue;
      if (EXCLUDE.has(key)) continue;
      toRemove.push(key);
    }
    for (const k of toRemove) localStorage.removeItem(k);
    for (const [k, v] of Object.entries(snapshot)) {
      if (EXCLUDE.has(k)) continue;
      localStorage.setItem(k, v);
    }
  } catch {
  }
  // Reload so module-scoped stores reinit cleanly.
  window.location.reload();
}
