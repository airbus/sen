// === color_scheme.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect } from "react";

import { makeStore, useSelector } from "@sen/client/react";

// Drives the `theme-light` class on <html>; consumers should read theme via CSS variables,
// not by branching on this value. Persisted in localStorage; no system-preference mirroring.

export type ColorScheme = "dark" | "light";

const STORAGE_KEY = "webex.colorScheme";

function loadFromStorage(): ColorScheme {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return raw === "light" ? "light" : "dark";
  } catch {
    return "dark";
  }
}

const schemeStore = makeStore<ColorScheme>(loadFromStorage());

// Apply at module load so the first render sees CSS variables for the persisted scheme;
// UPlotChart bakes variable values into its canvas at construction time, so a later
// effect would let charts launch with the previous scheme's colors.
if (typeof document !== "undefined") {
  applyColorSchemeClass(document, schemeStore.getState());
}

let lastPersisted = schemeStore.getState();
schemeStore.subscribe(() => {
  const current = schemeStore.getState();
  if (current === lastPersisted) return;
  lastPersisted = current;
  // Must run before React's re-render: child effects (UPlotChart's chart-build) read
  // CSS variables, and React runs child effects before parent effects, so a parent-level
  // effect in App can't apply the class in time.
  if (typeof document !== "undefined") {
    applyColorSchemeClass(document, current);
  }
  try {
    localStorage.setItem(STORAGE_KEY, current);
  } catch {
  }
});

export const colorSchemeActions = Object.freeze({
  set: (next: ColorScheme) => schemeStore.setState(() => next),
  toggle: () => schemeStore.setState((prev) => (prev === "dark" ? "light" : "dark")),
});

export function useColorScheme(): ColorScheme {
  return useSelector(schemeStore, (s) => s);
}

export function applyColorSchemeClass(doc: Document, scheme: ColorScheme): void {
  const root = doc.documentElement;
  if (scheme === "light") root.classList.add("theme-light");
  else root.classList.remove("theme-light");
}

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    schemeStore.dispose();
  });
}

/** Mirror the color scheme onto a popout's document. */
export function useApplyColorSchemeToPopout(doc: Document | null): void {
  const scheme = useColorScheme();
  useEffect(() => {
    if (!doc) return;
    applyColorSchemeClass(doc, scheme);
  }, [doc, scheme]);
}
