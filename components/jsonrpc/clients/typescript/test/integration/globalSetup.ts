// === globalSetup.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Vitest globalSetup for integration tests. Spawns a Sen subprocess against the YAML config
// supplied by SEN_CONFIG, waits for the WebSocket port to be open, returns a teardown function
// that signals the subprocess down on POSIX and TerminateProcess on Windows.
//
// SEN_BINARY and SEN_CONFIG are required, supplied by the CMake target
// jsonrpc_ts_client_integration (see CMakeLists.txt). There are no hard-coded fallbacks: the
// binary location depends on the build profile (gcc/clang/msvc, Debug/Release, custom presets),
// and CMake already knows the answer authoritatively via $<TARGET_FILE:sen::cli_sen>. Running
// this suite outside ctest is supported; set the two vars by hand.
//   SEN_BINARY  full path to the `sen` executable
//   SEN_CONFIG  full path to the YAML config (typically test/integration/configs/inheritance.yaml)
//   SEN_PORT    port the WebSocket server binds to (default 8080; must match the config)

import { senPort } from "./helpers.js";
import { spawnSen, type SenHandle } from "./subprocess.js";

function requiredEnv(name: string): string {
  const v = process.env[name];
  if (!v) {
    throw new Error(
      `${name} is not set. The CMake target jsonrpc_ts_client_integration sets it ` +
        `automatically; to run this suite outside ctest, set both SEN_BINARY and SEN_CONFIG by ` +
        `hand (e.g. SEN_BINARY=<build>/bin/sen ` +
        `SEN_CONFIG=<repo>/components/jsonrpc/clients/typescript/test/integration/configs/inheritance.yaml).`,
    );
  }
  return v;
}

let handle: SenHandle | undefined;

export async function setup(): Promise<void> {
  handle = await spawnSen({
    binary: requiredEnv("SEN_BINARY"),
    config: requiredEnv("SEN_CONFIG"),
    port: senPort,
  });
}

export async function teardown(): Promise<void> {
  await handle?.stop();
}
