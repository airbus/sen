// === event_store.test.ts =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeEach } from "vitest";

import {
  __appendEventForTests,
  __getAllEventsForTests,
  __getObjectEventsForTests,
  __resetEventStoreForTests,
  __trimOnceForTests,
  setEventRetentionSeconds,
  type EventDelivery,
} from "../../src/state/event_store.js";

function delivery(over: Partial<EventDelivery> = {}): EventDelivery {
  const seq = over.seq ?? 1;
  const interestName = over.interestName ?? "i";
  const objectName = over.objectName ?? "obj";
  const className = over.className ?? "demo.Aircraft";
  const eventName = over.eventName ?? "madeSound";
  return {
    seq,
    interestName,
    objectName,
    className,
    eventName,
    args: over.args ?? [],
    timestamp: over.timestamp ?? "2026-06-04 12:00:00 000000",
    at: over.at ?? Date.now(),
    lowerSearch: `${eventName}\0${objectName}\0${className}\0${interestName}`.toLowerCase(),
  };
}

describe("event_store", () => {
  beforeEach(__resetEventStoreForTests);

  describe("append + fan-out", () => {
    it("an appended event lands in both the per-object buffer and the global log", () => {
      __appendEventForTests(delivery({ seq: 1, eventName: "ev" }));
      expect(__getAllEventsForTests()).toHaveLength(1);
      expect(__getObjectEventsForTests("i", "obj")).toHaveLength(1);
    });

    it("events from different objects share the global log but stay separate per-object", () => {
      __appendEventForTests(delivery({ seq: 1, objectName: "a" }));
      __appendEventForTests(delivery({ seq: 2, objectName: "b" }));
      __appendEventForTests(delivery({ seq: 3, objectName: "a" }));
      expect(__getAllEventsForTests()).toHaveLength(3);
      expect(__getObjectEventsForTests("i", "a")).toHaveLength(2);
      expect(__getObjectEventsForTests("i", "b")).toHaveLength(1);
    });

    it("seq order in the global log reflects append order", () => {
      __appendEventForTests(delivery({ seq: 1, eventName: "a" }));
      __appendEventForTests(delivery({ seq: 2, eventName: "b" }));
      __appendEventForTests(delivery({ seq: 3, eventName: "c" }));
      const seqs = __getAllEventsForTests().map((e) => e.seq);
      expect(seqs).toEqual([1, 2, 3]);
    });
  });

  describe("freeze-snapshot semantics", () => {
    it(".slice() at capture time freezes the snapshot; the live array keeps appending", () => {
      __appendEventForTests(delivery({ seq: 1 }));
      __appendEventForTests(delivery({ seq: 2 }));
      // Consumer captures a frozen view (matches EventsWorkspace's pause behavior).
      const frozen = __getAllEventsForTests().slice();
      expect(frozen).toHaveLength(2);
      __appendEventForTests(delivery({ seq: 3 }));
      __appendEventForTests(delivery({ seq: 4 }));
      // Frozen view stays at 2 entries; the live array now has 4.
      expect(frozen).toHaveLength(2);
      expect(__getAllEventsForTests()).toHaveLength(4);
    });

    it("a held reference to the live array WITHOUT .slice() observes later mutations", () => {
      // Documents the trap the EventsWorkspace.pause bug fell into.
      __appendEventForTests(delivery({ seq: 1 }));
      const live = __getAllEventsForTests(); // no slice; direct reference to backing
      __appendEventForTests(delivery({ seq: 2 }));
      expect(live).toHaveLength(2);
    });
  });

  describe("retention trim", () => {
    it("drops events older than retentionSeconds on trim", () => {
      // trimOnce reads `Date.now()` at the moment it runs, so `at` values must be anchored
      // to wall-clock time for the comparison to be meaningful.
      setEventRetentionSeconds(1);
      const now = Date.now();
      __appendEventForTests(delivery({ seq: 1, at: now - 5000 })); // 5s old: dropped
      __appendEventForTests(delivery({ seq: 2, at: now - 500 })); // within window
      __appendEventForTests(delivery({ seq: 3, at: now }));
      __trimOnceForTests();
      const remaining = __getAllEventsForTests().map((e) => e.seq);
      expect(remaining).not.toContain(1);
      expect(remaining).toContain(2);
      expect(remaining).toContain(3);
    });

    it("trim empties a per-object buffer and drops the bucket", () => {
      setEventRetentionSeconds(1);
      // `at: 0` is the epoch: older than any reasonable Date.now()-1s cutoff.
      __appendEventForTests(delivery({ seq: 1, at: 0 }));
      __trimOnceForTests();
      expect(__getObjectEventsForTests("i", "obj")).toHaveLength(0);
    });
  });
});
