// === time.ts =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { parseSenTimestamp } from "@sen/client";

/** Sub-ms precision is dropped. */
export function parseServerTimestampMs(wire: string): number | null {
  const parsed = parseSenTimestamp(wire);
  return parsed ? parsed.date.getTime() : null;
}
