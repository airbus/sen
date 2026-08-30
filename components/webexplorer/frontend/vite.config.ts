// === vite.config.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { defineConfig, type Plugin } from "vite";
import react from "@vitejs/plugin-react";
import { viteSingleFile } from "vite-plugin-singlefile";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

// Offline-by-design: every asset is bundled, inlined, or self-hosted. The production build
// emits a single self-contained `dist/index.html` (all JS / CSS inlined via
// vite-plugin-singlefile) so it can be embedded into a Sen C++ component as a byte array
// (via `sen_file_to_cpp`) and registered with the jsonrpc `StaticFileServer` at runtime.
// See docs/components/webexplorer.md, "Offline behavior".
//
// Dev mode (`npm run dev`) uses chunked output + HMR; the singlefile plugin only applies to
// production builds.

// Emit dist/meta.json in esbuild metafile shape (only the `inputs` keys are populated, and
// only entries whose path lies under node_modules). Sen's CMake helper reads this to narrow
// license attribution to packages whose sources actually shipped.
function bundleMetafilePlugin(): Plugin {
  return {
    name: "sen-bundle-metafile",
    apply: "build",
    generateBundle() {
      const inputs: Record<string, Record<string, never>> = {};
      for (const id of this.getModuleIds()) {
        if (id.includes("/node_modules/")) {
          const idx = id.indexOf("/node_modules/");
          inputs[id.slice(idx + 1)] = {};
        }
      }
      const outPath = join(__dirname, "dist", "meta.json");
      mkdirSync(dirname(outPath), { recursive: true });
      writeFileSync(outPath, JSON.stringify({ inputs }, null, 2));
    },
  };
}

export default defineConfig({
  plugins: [react(), viteSingleFile(), bundleMetafilePlugin()],
  server: {
    port: 5173,
    strictPort: true,
  },
  build: {
    target: "es2022",
    cssCodeSplit: false,
    assetsInlineLimit: 100_000_000,
    rollupOptions: {
      output: {
        inlineDynamicImports: true,
      },
    },
  },
});
