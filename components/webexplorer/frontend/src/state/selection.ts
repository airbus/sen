// === selection.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Single-selection model. Keyed by (interest, object) so the selection survives churn
// within an interest and is invalidated when the interest is released.

export interface Selection {
  interestName: string;
  objectName: string;
  className: string;
  /** Empty when the interest's query has no FROM clause. */
  sessionName: string;
  busName: string;
}
