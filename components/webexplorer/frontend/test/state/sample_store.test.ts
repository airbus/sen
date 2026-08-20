// === sample_store.test.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeEach } from "vitest";

import {
  __getLeafCountForTests,
  __getSampleViewForTests,
  __reportDeliveryForTests,
  __resetSampleStoreForTests,
  __trimOnceForTests,
  markDeadLeaf,
  markDeadProperty,
  setSampleRetentionSeconds,
} from "../../src/state/sample_store.js";
import type { PropertyKey } from "../../src/core/watch_keys.js";

const SOURCE = "i\0obj\0altitude"; // PropertyKey shape: interest\0object\0property

describe("sample_store", () => {
  beforeEach(__resetSampleStoreForTests);

  describe("append + view", () => {
    it("reports a sample and exposes it via getView", () => {
      __reportDeliveryForTests(SOURCE, [["", 42, "m"]], 1000, 950);
      const view = __getSampleViewForTests(SOURCE, "");
      expect(view.size).toBe(1);
      expect(view.vKind).toBe("number");
      expect(view.vNum![0]).toBe(42);
      expect(view.unit).toBe("m");
    });

    it("appending multiple samples increments size with shared typed-array storage", () => {
      __reportDeliveryForTests(SOURCE, [["", 1, null]], 100, 90);
      __reportDeliveryForTests(SOURCE, [["", 2, null]], 200, 190);
      __reportDeliveryForTests(SOURCE, [["", 3, null]], 300, 290);
      const view = __getSampleViewForTests(SOURCE, "");
      expect(view.size).toBe(3);
      expect(view.vNum![0]).toBe(1);
      expect(view.vNum![1]).toBe(2);
      expect(view.vNum![2]).toBe(3);
    });

    it("the BufferView is mutable backing - later appends mutate the same typed arrays", () => {
      // Documents the same trap the freeze-snapshot bug fell into: holding a BufferView ref
      // does NOT freeze the data.
      __reportDeliveryForTests(SOURCE, [["", 1, null]], 100, null);
      const view = __getSampleViewForTests(SOURCE, "");
      expect(view.size).toBe(1);
      __reportDeliveryForTests(SOURCE, [["", 2, null]], 200, null);
      // Reading the SAME view object reflects the new sample because vNum is the live array
      // and size was bumped on the buffer. The view's `size` field is captured at view
      // creation time though, so we re-read here:
      const viewAfter = __getSampleViewForTests(SOURCE, "");
      expect(viewAfter.size).toBe(2);
      // The new view is a fresh wrapper but its `vNum` is the same backing array as view.vNum.
      expect(viewAfter.vNum).toBe(view.vNum);
    });
  });

  describe("retention trim", () => {
    it("drops samples older than retentionSeconds, keeping at least one", () => {
      setSampleRetentionSeconds(1);
      const now = Date.now();
      __reportDeliveryForTests(SOURCE, [["", 1, null]], now - 5000, null); // 5s old
      __reportDeliveryForTests(SOURCE, [["", 2, null]], now - 500, null);
      __reportDeliveryForTests(SOURCE, [["", 3, null]], now, null);
      __trimOnceForTests();
      const view = __getSampleViewForTests(SOURCE, "");
      expect(view.size).toBeGreaterThanOrEqual(2);
      // The very old sample is gone.
      expect(view.vNum![0]).not.toBe(1);
    });

    it("evicts the leaf entirely when it has been quiet for 2x retention", () => {
      setSampleRetentionSeconds(1);
      __reportDeliveryForTests(SOURCE, [["", 1, null]], Date.now() - 10_000, null);
      expect(__getLeafCountForTests()).toBe(1);
      __trimOnceForTests();
      expect(__getLeafCountForTests()).toBe(0);
    });
  });

  describe("multi-leaf state", () => {
    it("samples are kept independently per (sourceKey, leafPath)", () => {
      __reportDeliveryForTests(SOURCE, [["x", 10, null], ["y", 20, null]], 100, null);
      __reportDeliveryForTests(SOURCE, [["x", 11, null]], 200, null);
      const xView = __getSampleViewForTests(SOURCE, "x");
      const yView = __getSampleViewForTests(SOURCE, "y");
      expect(xView.size).toBe(2);
      expect(yView.size).toBe(1);
    });
  });

  describe("gap sentinels", () => {
    it("markDeadLeaf appends a sentinel sample with vGap[i] = 1", () => {
      __reportDeliveryForTests(SOURCE, [["", 42, null]], 100, null);
      markDeadLeaf(SOURCE as PropertyKey, "", 200);
      const view = __getSampleViewForTests(SOURCE, "");
      expect(view.size).toBe(2);
      expect(view.vGap![0]).toBe(0);
      expect(view.vGap![1]).toBe(1);
      expect(Number.isNaN(view.vNum![1]!)).toBe(true);
      expect(view.at[1]).toBe(200);
    });

    it("markDeadLeaf is a no-op on an empty buffer (no value to gap from)", () => {
      markDeadLeaf(SOURCE as PropertyKey, "missing", 200);
      const view = __getSampleViewForTests(SOURCE, "missing");
      expect(view.size).toBe(0);
    });

    it("markDeadLeaf is idempotent - successive calls collapse to a single sentinel", () => {
      __reportDeliveryForTests(SOURCE, [["", 1, null]], 100, null);
      markDeadLeaf(SOURCE as PropertyKey, "", 200);
      markDeadLeaf(SOURCE as PropertyKey, "", 250);
      markDeadLeaf(SOURCE as PropertyKey, "", 300);
      const view = __getSampleViewForTests(SOURCE, "");
      expect(view.size).toBe(2); // one real + one sentinel; further markDead's no-op
      expect(view.at[1]).toBe(200); // first sentinel's timestamp wins
    });

    it("a real sample after a sentinel resumes the series; later markDead gaps again", () => {
      __reportDeliveryForTests(SOURCE, [["", 1, null]], 100, null);
      markDeadLeaf(SOURCE as PropertyKey, "", 200);
      __reportDeliveryForTests(SOURCE, [["", 3, null]], 300, null);
      markDeadLeaf(SOURCE as PropertyKey, "", 400);
      const view = __getSampleViewForTests(SOURCE, "");
      expect(view.size).toBe(4);
      expect(view.vGap![0]).toBe(0); // real
      expect(view.vGap![1]).toBe(1); // sentinel
      expect(view.vGap![2]).toBe(0); // resumed real
      expect(view.vGap![3]).toBe(1); // sentinel again
    });

    it("markDeadProperty marks every leaf of one property dead in one call", () => {
      __reportDeliveryForTests(SOURCE, [["x", 1, null], ["y", 2, null]], 100, null);
      markDeadProperty(SOURCE as PropertyKey, 200);
      const xView = __getSampleViewForTests(SOURCE, "x");
      const yView = __getSampleViewForTests(SOURCE, "y");
      expect(xView.size).toBe(2);
      expect(yView.size).toBe(2);
      expect(xView.vGap![1]).toBe(1);
      expect(yView.vGap![1]).toBe(1);
    });

    it("works for boolean and string-kinded buffers too (gap flag, not value)", () => {
      __reportDeliveryForTests(SOURCE, [["b", true, null]], 100, null);
      __reportDeliveryForTests(SOURCE, [["s", "ok", null]], 100, null);
      markDeadLeaf(SOURCE as PropertyKey, "b", 200);
      markDeadLeaf(SOURCE as PropertyKey, "s", 200);
      const bView = __getSampleViewForTests(SOURCE, "b");
      const sView = __getSampleViewForTests(SOURCE, "s");
      expect(bView.vGap![1]).toBe(1);
      expect(sView.vGap![1]).toBe(1);
      // Value column gets a zero-ish placeholder; consumers must read vGap, not value.
      expect(bView.vBool![1]).toBe(0);
      expect(sView.vStr![1]).toBe("");
    });
  });
});
