// === vitest.config.ts ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    environment: "node",
    include: ["test/**/*.test.ts"],
    // Integration tests live alongside the unit tests but need a running Sen subprocess. They
    // ship under their own vitest config (vitest.integration.config.ts) with a globalSetup; the
    // default unit-test loop excludes them so `npm test` stays fast and self-contained.
    exclude: ["test/integration/**", "node_modules/**", "dist/**"],
    globals: false,
  },
});
