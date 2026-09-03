// === ring_buffer.test.ts =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { RingBuffer } from "../../src/util/ring_buffer.js";

describe("RingBuffer", () => {
  it("rejects non-positive cap", () => {
    expect(() => new RingBuffer<number>(0)).toThrow();
    expect(() => new RingBuffer<number>(-1)).toThrow();
  });

  it("drains empty as []", () => {
    const b = new RingBuffer<number>(4);
    expect(b.size).toBe(0);
    expect(b.drainAll()).toEqual([]);
  });

  it("preserves FIFO order under cap", () => {
    const b = new RingBuffer<number>(4);
    b.push(1);
    b.push(2);
    b.push(3);
    expect(b.size).toBe(3);
    expect(b.drainAll()).toEqual([1, 2, 3]);
    expect(b.size).toBe(0);
  });

  it("preserves FIFO order at exactly full", () => {
    const b = new RingBuffer<number>(3);
    b.push(1);
    b.push(2);
    b.push(3);
    expect(b.size).toBe(3);
    expect(b.drainAll()).toEqual([1, 2, 3]);
  });

  it("drops oldest on overflow", () => {
    const b = new RingBuffer<number>(3);
    b.push(1);
    b.push(2);
    b.push(3);
    b.push(4);
    expect(b.size).toBe(3);
    expect(b.drainAll()).toEqual([2, 3, 4]);
  });

  it("drops oldest across multiple overflows", () => {
    const b = new RingBuffer<number>(3);
    for (let i = 1; i <= 10; i++) b.push(i);
    expect(b.size).toBe(3);
    expect(b.drainAll()).toEqual([8, 9, 10]);
  });

  it("drain resets head + size so subsequent pushes start fresh", () => {
    const b = new RingBuffer<number>(3);
    b.push(1);
    b.push(2);
    b.drainAll();
    b.push(10);
    b.push(20);
    expect(b.drainAll()).toEqual([10, 20]);
  });

  it("releases drained references for GC", () => {
    // Can't observe slot-clearing directly; a follow-up push must not co-mingle the
    // previous reference into the same slot.
    const b = new RingBuffer<{ tag: string }>(2);
    const a = { tag: "a" };
    b.push(a);
    b.drainAll();
    const c = { tag: "c" };
    b.push(c);
    const drained = b.drainAll();
    expect(drained).toEqual([c]);
    expect(drained[0]).toBe(c);
  });

  it("handles cap=1 (degenerate ring)", () => {
    const b = new RingBuffer<number>(1);
    b.push(1);
    b.push(2);
    b.push(3);
    expect(b.size).toBe(1);
    expect(b.drainAll()).toEqual([3]);
  });
});

describe("RingBuffer byte bound", () => {
  const bySize = { maxBytes: 10, sizeOf: (n: number) => n };

  it("rejects a non-positive budget", () => {
    expect(() => new RingBuffer<number>(4, { maxBytes: 0, sizeOf: () => 1 })).toThrow();
  });

  it("counts nothing when no bound is configured", () => {
    const b = new RingBuffer<number>(4);
    b.push(1);
    expect(b.bytes).toBe(0);
  });

  it("drops oldest until the newest entry fits the budget", () => {
    const b = new RingBuffer<number>(100, bySize);
    b.push(4);
    b.push(4);
    expect(b.size).toBe(2);
    b.push(4);
    expect(b.drainAll()).toEqual([4, 4]);
  });

  it("keeps an oversized entry rather than dropping everything", () => {
    const b = new RingBuffer<number>(100, bySize);
    b.push(3);
    b.push(99);
    expect(b.drainAll()).toEqual([99]);
  });

  it("tracks the running total across drops and drains", () => {
    const b = new RingBuffer<number>(3, bySize);
    b.push(2);
    b.push(3);
    expect(b.bytes).toBe(5);
    b.push(4);
    b.push(1);
    expect(b.bytes).toBe(8);
    expect(b.size).toBe(3);
    b.drainAll();
    expect(b.bytes).toBe(0);
    b.push(2);
    expect(b.bytes).toBe(2);
  });

  it("still honours the entry cap when every entry is tiny", () => {
    const b = new RingBuffer<number>(3, { maxBytes: 1_000, sizeOf: () => 1 });
    for (let i = 1; i <= 6; i++) b.push(i);
    expect(b.drainAll()).toEqual([4, 5, 6]);
  });
});
