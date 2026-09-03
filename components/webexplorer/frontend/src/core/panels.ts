// === panels.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// A panel is one kind for its lifetime; adding a series of a different kind needs a new panel.
//   numeric  - line plot of numbers / Quantity values.
//   discrete - stepped lines mapped to states (booleans, enums, variant tags, ...).
export type PanelKind = "numeric" | "discrete";
