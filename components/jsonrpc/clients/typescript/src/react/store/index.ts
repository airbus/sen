// === index.ts ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Reactive-store primitive for Sen-based React apps. Re-exported from `@sen/client/react`;
// the typical consumer imports from there, not from this subpath directly.

export { makeStore, type Store } from "./make_store.js";
export {
  makeBufferStore,
  type BufferStore,
  type BufferStoreOptions,
} from "./make_buffer_store.js";
export {
  makeBufferCell,
  type BufferCell,
  type BufferCellOptions,
} from "./make_buffer_cell.js";
export { useStore, useSelector } from "./use_store.js";
export { useView, useViews } from "./use_view.js";
export { useCell } from "./use_cell.js";
