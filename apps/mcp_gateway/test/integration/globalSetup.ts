// === globalSetup.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { senPort } from "./helpers.js";
import { spawnSen, type SenHandle } from "./spawn_sen.js";

let handle: SenHandle | undefined;

export async function setup(): Promise<void> {
  const binary = process.env["SEN_BINARY"];
  const config = process.env["SEN_CONFIG"];
  if (binary === undefined || config === undefined || binary === "" || config === "") {
    throw new Error(
      "globalSetup: SEN_BINARY and SEN_CONFIG must be set. " +
        "From CMake, ctest sets these automatically; for direct `npm run test:integration`, " +
        "export them before running.",
    );
  }
  handle = await spawnSen({ binary, config, port: senPort });
}

export async function teardown(): Promise<void> {
  await handle?.stop();
}
