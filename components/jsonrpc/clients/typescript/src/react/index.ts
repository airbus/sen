// === index.ts ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Public entry point for the React adapter (`@sen/client/react`). Thin reactive bindings over
// the Layer-3 surface; React is a peer dependency, kept out of the core import path so non-React
// consumers pay no cost.

export { useConnectionState } from "./use_connection_state.js";
export { useEvents, type EventDelivery, type UseEventsOptions } from "./use_events.js";
export { useInterest, type UseInterestArgs, type UseInterestResult } from "./use_interest.js";
export { useInvoke, type InvokeStatus, type UseInvokeResult } from "./use_invoke.js";
export { useObject } from "./use_object.js";
export { useObjects } from "./use_objects.js";
export { useProperty } from "./use_property.js";
export { useSetProperty, type SetPropertyStatus, type UseSetPropertyResult } from "./use_set_property.js";
export { useTopology, type UseTopologyResult } from "./use_topology.js";
export { useTypeSpec } from "./use_type_spec.js";

// Reactive store primitive. See `store/README.md` for the API surface.
export {
  makeStore,
  makeBufferStore,
  makeBufferCell,
  useStore,
  useSelector,
  useView,
  useViews,
  useCell,
  type Store,
  type BufferStore,
  type BufferStoreOptions,
  type BufferCell,
  type BufferCellOptions,
} from "./store/index.js";
