// === make_buffer_cell.test.ts ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, vi } from "vitest";

import { makeBufferCell } from "../../src/react/store/make_buffer_cell.js";

describe("makeBufferCell", () => {
  it("seeds via the producer on first read and caches the result", () => {
    let calls = 0;
    const cell = makeBufferCell<number>({
      produce: () => ++calls,
      rafBatch: false,
    });
    const a = cell.getView();
    const b = cell.getView();
    expect(a).toBe(b);
    expect(calls).toBe(1);
  });

  it("invalidate clears the cache and fires subscribers (synchronous flush)", () => {
    let calls = 0;
    const cell = makeBufferCell<number>({
      produce: () => ++calls,
      rafBatch: false,
    });
    const listener = vi.fn();
    cell.subscribe(listener);

    cell.getView();
    cell.invalidate();
    expect(listener).toHaveBeenCalledTimes(1);
    expect(cell.getView()).toBe(2);
  });

  it("subscribe cancel removes only that listener", () => {
    const cell = makeBufferCell<number>({
      produce: () => 0,
      rafBatch: false,
    });
    const kept = vi.fn();
    const dropped = vi.fn();
    cell.subscribe(kept);
    const cancel = cell.subscribe(dropped);
    cancel();
    cell.invalidate();
    expect(kept).toHaveBeenCalledTimes(1);
    expect(dropped).not.toHaveBeenCalled();
  });

  it("dispose silences subsequent invalidates", () => {
    const cell = makeBufferCell<number>({
      produce: () => 0,
      rafBatch: false,
    });
    const listener = vi.fn();
    cell.subscribe(listener);

    cell.dispose();
    cell.invalidate();
    expect(listener).not.toHaveBeenCalled();
  });

  it("the wrapper-around-mutable-backing convention works for zero-copy reads", () => {
    // Pattern from the RFC: producer wraps a live mutable array. The wrapper identity
    // flips per invalidate; the array reference inside is shared (no .slice).
    const backing: number[] = [];
    const cell = makeBufferCell<{ items: readonly number[] }>({
      produce: () => ({ items: backing }),
      rafBatch: false,
    });

    const v1 = cell.getView();
    backing.push(1, 2, 3);
    // Until invalidate fires, the cell returns the cached wrapper. The wrapper's items ref
    // points at the live backing, so the consumer sees the appended values via the same
    // wrapper. This matches event_store's intended consumption pattern.
    expect(v1.items).toEqual([1, 2, 3]);

    cell.invalidate();
    const v2 = cell.getView();
    expect(v2).not.toBe(v1); // fresh wrapper after invalidate
    expect(v2.items).toBe(backing); // same backing
    expect(v2.items).toEqual([1, 2, 3]);

    // Append more, invalidate, observe.
    backing.push(4);
    cell.invalidate();
    const v3 = cell.getView();
    expect(v3.items).toEqual([1, 2, 3, 4]);
  });
});
