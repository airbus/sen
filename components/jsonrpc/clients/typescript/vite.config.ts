// === vite.config.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { defineConfig } from "vite";
import dts from "vite-plugin-dts";
import { resolve } from "node:path";

export default defineConfig({
  build: {
    lib: {
      // Two entry points: the core (`@sen/client`) and the React adapter
      // (`@sen/client/react`). The core has no React dependency at runtime.
      entry: {
        index: resolve(__dirname, "src/index.ts"),
        "react/index": resolve(__dirname, "src/react/index.ts"),
      },
      formats: ["es"],
    },
    sourcemap: true,
    rollupOptions: {
      // React is a peer dep of the `/react` sub-import; never bundle it.
      external: ["ws", "react"],
    },
  },
  plugins: [
    dts({
      entryRoot: "src",
      include: ["src/**/*.ts"],
      tsconfigPath: "./tsconfig.json",
    }),
  ],
});
