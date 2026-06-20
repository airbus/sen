// === cancel.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { CancelFn } from "../values.js";

/** Wrap a disposer so it runs at most once. */
export function makeIdempotent(dispose: () => void): CancelFn {
  let done = false;
  return () => {
    if (done) return;
    done = true;
    dispose();
  };
}
