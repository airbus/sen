// === report_error.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { ReportError } from "../connect.js";

/** JS permits `throw "x"`; normalize to an Error so consumers can rely on the shape. */
export function normalizeError(err: unknown): Error {
  if (err instanceof Error) return err;
  return new Error(String(err));
}

/** Default reporter when {@link ClientOptions.onError} is not provided. */
export const defaultReportError: ReportError = (err) => {
  console.error(err);
};
