// === make_buffer_cell.ts =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { CancelFn } from "../../values.js";
import { makeBufferStore, type BufferStoreOptions } from "./make_buffer_store.js";

/**
 * Single-cell buffer store: same semantics as {@link BufferStore} but with a single
 * implicit key. Use this when there is no per-key dimension to the data (e.g., a global
 * merged event log, a session-wide aggregate).
 *
 * @example
 *   const allEvents: EventDelivery[] = [];
 *   const cell = makeBufferCell({ produce: () => ({ events: allEvents }) });
 *   // On append:
 *   allEvents.push(event);
 *   cell.invalidate();
 *   // Consumer:
 *   const { events } = cell.getView();
 */
export type BufferCellOptions<T> = Omit<BufferStoreOptions<symbol, T>, "produce"> & {
  produce: () => T;
};

/** Single-cell variant of {@link BufferStore}. See {@link makeBufferCell}. */
export interface BufferCell<T> {
  /** Same reference until {@link invalidate} is called. */
  getView(): T;
  /** Schedule a flush; subscribers fire once per rAF (or synchronously when rafBatch is false). */
  invalidate(): void;
  /** Subscribe to invalidations. */
  subscribe(listener: () => void): CancelFn;
  /** Clear listeners and cancel any pending flush. */
  dispose(): void;
}

/** The single sentinel key used by every {@link BufferCell} under the hood. */
const CELL_KEY: unique symbol = Symbol("BufferCell");

/**
 * Create a {@link BufferCell}. Implemented as a thin wrapper over {@link makeBufferStore}
 * with one sentinel key; no separate code path.
 */
export function makeBufferCell<T>(opts: BufferCellOptions<T>): BufferCell<T> {
  const inner = makeBufferStore<symbol, T>({
    produce: () => opts.produce(),
    ...(opts.rafBatch !== undefined ? { rafBatch: opts.rafBatch } : {}),
  });
  return {
    getView: () => inner.getView(CELL_KEY),
    invalidate: () => inner.invalidate(CELL_KEY),
    subscribe: (l) => inner.subscribeKey(CELL_KEY, l),
    dispose: () => inner.dispose(),
  };
}
