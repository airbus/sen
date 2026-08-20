// === vitest.integration.config.ts ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Integration suite: spawns a Sen subprocess against the SEN_CONFIG YAML, runs the per-feature
// tests under test/integration/, then tears the subprocess down. Driven by `npm run
// test:integration`; separate from the unit-test loop (vitest.config.ts) so the fast path stays
// mock-only.

import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    environment: "node",
    include: ["test/integration/**/*.test.ts"],
    globalSetup: ["test/integration/globalSetup.ts"],
    globals: false,
    // Subprocess spawn + first connect can take a second; the per-test timeout default is
    // generous to absorb that.
    testTimeout: 10_000,
    // Tests share one Sen subprocess (single fork). Sequential execution avoids interleaved
    // notifications and shared-port races.
    sequence: { concurrent: false },
    pool: "forks",
    poolOptions: { forks: { singleFork: true } },
  },
});
