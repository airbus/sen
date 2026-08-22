// === ring_buffer.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Second bound for entries whose size varies: an entry count alone leaves the memory a full
// buffer holds open-ended. `sizeOf` may be a rough estimate, so treat `maxBytes` as a target
// rather than a guarantee.
export interface ByteBound<T> {
  maxBytes: number;
  sizeOf: (entry: T) => number;
}

// Drops oldest on overflow, of either bound.
export class RingBuffer<T> {
  private readonly slots: Array<T | undefined>;
  private readonly sizes: number[];
  private readonly maxBytes: number;
  private readonly sizeOf: (entry: T) => number;
  private head = 0;
  private count = 0;
  private byteTotal = 0;

  constructor(
    private readonly cap: number,
    byteBound?: ByteBound<T>,
  ) {
    if (cap <= 0) throw new Error(`RingBuffer cap must be positive (got ${cap})`);
    if (byteBound !== undefined && byteBound.maxBytes <= 0) {
      throw new Error(`RingBuffer maxBytes must be positive (got ${byteBound.maxBytes})`);
    }
    this.slots = new Array<T | undefined>(cap).fill(undefined);
    this.sizes = new Array<number>(cap).fill(0);
    this.maxBytes = byteBound?.maxBytes ?? Number.POSITIVE_INFINITY;
    this.sizeOf = byteBound?.sizeOf ?? (() => 0);
  }

  push(entry: T): void {
    const size = this.sizeOf(entry);
    if (this.count === this.cap) this.dropOldest();
    // The newest entry always gets in, even alone over budget: a caller polling for the latest
    // delivery would otherwise see nothing at all.
    while (this.count > 0 && this.byteTotal + size > this.maxBytes) this.dropOldest();
    this.slots[this.head] = entry;
    this.sizes[this.head] = size;
    this.head = (this.head + 1) % this.cap;
    this.count += 1;
    this.byteTotal += size;
  }

  get size(): number {
    return this.count;
  }

  get bytes(): number {
    return this.byteTotal;
  }

  drainAll(): T[] {
    if (this.count === 0) return [];
    const out = new Array<T>(this.count);
    const start = (this.head - this.count + this.cap) % this.cap;
    for (let i = 0; i < this.count; i++) {
      const idx = (start + i) % this.cap;
      out[i] = this.slots[idx]!;
      this.slots[idx] = undefined;
      this.sizes[idx] = 0;
    }
    this.count = 0;
    this.head = 0;
    this.byteTotal = 0;
    return out;
  }

  private dropOldest(): void {
    const idx = (this.head - this.count + this.cap) % this.cap;
    this.slots[idx] = undefined;
    this.byteTotal -= this.sizes[idx]!;
    this.sizes[idx] = 0;
    this.count -= 1;
  }
}
