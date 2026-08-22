// === helpers.ts ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

export const senPort = Number(process.env["SEN_PORT"] ?? 8080);
export const senUrl = `ws://127.0.0.1:${senPort}`;
