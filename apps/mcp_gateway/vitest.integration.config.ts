// === vitest.integration.config.ts ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Integration tests. Serial: parallel runs would multiplex stdout/stderr on the same
// gateway child and confuse JSON-RPC framing.

import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    include: ["test/integration/**/*.test.ts"],
    testTimeout: 30_000,
    hookTimeout: 30_000,
    sequence: { concurrent: false },
    pool: "forks",
    poolOptions: {
      forks: { singleFork: true },
    },
    globalSetup: ["test/integration/globalSetup.ts"],
  },
});
