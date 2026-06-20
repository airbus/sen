// === use_cell.ts =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useSyncExternalStore } from "react";

import type { BufferCell } from "./make_buffer_cell.js";

/**
 * Subscribe to a {@link BufferCell}. Returns the cell's current view; re-renders only when
 * the cell is invalidated.
 *
 * The cell wraps its data in a producer-returned shape (typically `{ items, ... }`); the
 * consumer unwraps via the documented field. See {@link makeBufferCell} for the wrapper
 * convention.
 *
 * @example
 *   const { events } = useCell(allEventsCell);
 */
export function useCell<T>(cell: BufferCell<T>): T {
  return useSyncExternalStore(cell.subscribe, cell.getView);
}
