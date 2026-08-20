// === watch_plot.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeEach } from "vitest";

import {
  __resetWatchPlotForTests,
  __getWatchPlotStateForTests,
  watchActions,
} from "../../src/state/watch_plot.js";
import { makePropertyKey, makeWatchKey, type WatchSource } from "../../src/core/watch_keys.js";

function source(
  propertyName: string,
  leafPath = "",
  overrides: Partial<WatchSource> = {},
): WatchSource {
  return {
    interestName: "i",
    objectName: "obj",
    className: "demo.Aircraft",
    propertyName,
    declaredType: "float64",
    sessionName: "sess",
    busName: "bus",
    leafPath,
    ...overrides,
  };
}

describe("watch_plot", () => {
  beforeEach(__resetWatchPlotForTests);

  describe("watch actions", () => {
    it("addWatch appends a source and updates the derived watchedKeys set", () => {
      const s = source("altitude");
      watchActions.addWatch(s);
      const state = __getWatchPlotStateForTests();
      expect(state.watchSources).toHaveLength(1);
      expect(state.watchedKeys.has(makeWatchKey(s))).toBe(true);
    });

    it("addWatch is idempotent on the same watch key", () => {
      const s = source("altitude");
      watchActions.addWatch(s);
      const first = __getWatchPlotStateForTests();
      watchActions.addWatch(s);
      const second = __getWatchPlotStateForTests();
      // setState returned `prev` on no-op, so state identity is preserved.
      expect(second).toBe(first);
      expect(second.watchSources).toHaveLength(1);
    });

    it("removeWatch drops the entry and updates watchedKeys", () => {
      const s = source("altitude");
      watchActions.addWatch(s);
      watchActions.removeWatch(makeWatchKey(s));
      const state = __getWatchPlotStateForTests();
      expect(state.watchSources).toHaveLength(0);
      expect(state.watchedKeys.size).toBe(0);
    });

    it("removeWatch is a no-op for an unknown key", () => {
      watchActions.addWatch(source("altitude"));
      const before = __getWatchPlotStateForTests();
      watchActions.removeWatch(makeWatchKey(source("speed")));
      const after = __getWatchPlotStateForTests();
      expect(after).toBe(before);
    });

    it("removeWatch prunes orphan plot series when no other watch covers the property", () => {
      const s = source("altitude");
      watchActions.addWatch(s);
      watchActions.togglePlottedLeaf(s, "");
      expect(__getWatchPlotStateForTests().panels).toHaveLength(1);
      watchActions.removeWatch(makeWatchKey(s));
      expect(__getWatchPlotStateForTests().panels).toHaveLength(0);
    });

    it("removeWatch keeps plot series when another watch entry still covers the property", () => {
      const whole = source("altitude", "");
      const leaf = source("altitude", "x");
      watchActions.addWatch(whole);
      watchActions.addWatch(leaf);
      watchActions.togglePlottedLeaf(whole, ""); // plot the root leaf
      // Removing the whole watch leaves the per-leaf watch covering the property.
      watchActions.removeWatch(makeWatchKey(whole));
      expect(__getWatchPlotStateForTests().panels).toHaveLength(1);
    });
  });

  describe("plot actions", () => {
    it("togglePlottedLeaf adds the leaf as a new panel and implicit watch", () => {
      const s = source("altitude");
      watchActions.togglePlottedLeaf(s, "");
      const state = __getWatchPlotStateForTests();
      expect(state.panels).toHaveLength(1);
      expect(state.panels[0]!.series).toHaveLength(1);
      // The plot adds an implicit whole-property watch when none exists.
      expect(state.watchedKeys.has(makeWatchKey({ ...s, leafPath: "" }))).toBe(true);
    });

    it("togglePlottedLeaf removes the panel when the leaf is already plotted", () => {
      const s = source("altitude");
      watchActions.togglePlottedLeaf(s, "");
      watchActions.togglePlottedLeaf(s, "");
      expect(__getWatchPlotStateForTests().panels).toHaveLength(0);
    });

    it("togglePlottedLeaf on a sub-leaf adds only the per-leaf watch (not whole)", () => {
      const s = source("position");
      watchActions.togglePlottedLeaf(s, "x");
      const state = __getWatchPlotStateForTests();
      expect(state.watchedKeys.has(makeWatchKey({ ...s, leafPath: "x" }))).toBe(true);
      expect(state.watchedKeys.has(makeWatchKey({ ...s, leafPath: "" }))).toBe(false);
    });

    it("togglePlottedLeaf with whole-property already watched does not add a per-leaf watch", () => {
      const s = source("position", "");
      watchActions.addWatch(s);
      watchActions.togglePlottedLeaf(s, "x");
      const state = __getWatchPlotStateForTests();
      // Whole-property watch covers the leaf transitively; no extra entry needed.
      expect(state.watchSources).toHaveLength(1);
      expect(state.watchedKeys.has(makeWatchKey({ ...s, leafPath: "" }))).toBe(true);
    });

    it("removeSeries drops the matching series and the panel if it becomes empty", () => {
      const s = source("altitude");
      watchActions.togglePlottedLeaf(s, "");
      watchActions.removeSeries(makePropertyKey(s), "");
      expect(__getWatchPlotStateForTests().panels).toHaveLength(0);
    });

    it("moveSeriesToPanel relocates a series into an existing panel", () => {
      const a = source("altitude");
      const b = source("speed");
      watchActions.togglePlottedLeaf(a, "");
      watchActions.togglePlottedLeaf(b, "");
      const before = __getWatchPlotStateForTests();
      expect(before.panels).toHaveLength(2);
      const targetPanel = before.panels[0]!.id;
      watchActions.moveSeriesToPanel(targetPanel, makePropertyKey(b), "");
      const after = __getWatchPlotStateForTests();
      expect(after.panels).toHaveLength(1);
      expect(after.panels[0]!.series).toHaveLength(2);
    });

    it("moveSeriesToNewPanel pulls a series back into its own panel", () => {
      const a = source("altitude");
      const b = source("speed");
      watchActions.togglePlottedLeaf(a, "");
      watchActions.togglePlottedLeaf(b, "");
      // Merge into one panel first.
      const firstPanelId = __getWatchPlotStateForTests().panels[0]!.id;
      watchActions.moveSeriesToPanel(firstPanelId, makePropertyKey(b), "");
      expect(__getWatchPlotStateForTests().panels).toHaveLength(1);
      // Now un-merge.
      watchActions.moveSeriesToNewPanel(makePropertyKey(b), "");
      expect(__getWatchPlotStateForTests().panels).toHaveLength(2);
    });
  });

  describe("batchEdit", () => {
    it("prunes plot series for a removed property that no remaining entry covers", () => {
      const s = source("altitude");
      watchActions.addWatch(s);
      watchActions.togglePlottedLeaf(s, "");
      expect(__getWatchPlotStateForTests().panels).toHaveLength(1);
      watchActions.batchEdit([makeWatchKey(s)], []);
      const state = __getWatchPlotStateForTests();
      expect(state.watchSources).toHaveLength(0);
      expect(state.panels).toHaveLength(0);
    });

    it("keeps plot series when a per-leaf watch still covers the dropped property", () => {
      const whole = source("altitude", "");
      const leaf = source("altitude", "x");
      watchActions.addWatch(whole);
      watchActions.addWatch(leaf);
      watchActions.togglePlottedLeaf(whole, ""); // plot the root
      // Drop only the whole-property watch; the per-leaf watch still covers the propertyKey.
      watchActions.batchEdit([makeWatchKey(whole)], []);
      const state = __getWatchPlotStateForTests();
      expect(state.watchSources).toHaveLength(1);
      expect(state.panels).toHaveLength(1);
    });

    it("dedupes additions that overlap existing watches", () => {
      const s = source("altitude");
      watchActions.addWatch(s);
      watchActions.batchEdit([], [s, source("speed")]);
      const state = __getWatchPlotStateForTests();
      // `altitude` already present, not re-appended; `speed` is new.
      expect(state.watchSources).toHaveLength(2);
      expect(state.watchedKeys.has(makeWatchKey(s))).toBe(true);
      expect(state.watchedKeys.has(makeWatchKey(source("speed")))).toBe(true);
    });
  });

  describe("derived sets", () => {
    it("watchedKeys identity is stable across no-op setState calls", () => {
      watchActions.addWatch(source("a"));
      const k1 = __getWatchPlotStateForTests().watchedKeys;
      watchActions.addWatch(source("a")); // duplicate -> no-op
      const k2 = __getWatchPlotStateForTests().watchedKeys;
      expect(k2).toBe(k1);
    });

    it("plottedLeaves identity flips when panels change, stable when they don't", () => {
      const s = source("altitude");
      watchActions.togglePlottedLeaf(s, "");
      const p1 = __getWatchPlotStateForTests().plottedLeaves;
      watchActions.addWatch(source("speed")); // doesn't touch panels
      const p2 = __getWatchPlotStateForTests().plottedLeaves;
      expect(p2).toBe(p1);
      watchActions.togglePlottedLeaf(source("speed"), "");
      const p3 = __getWatchPlotStateForTests().plottedLeaves;
      expect(p3).not.toBe(p1);
    });
  });
});
