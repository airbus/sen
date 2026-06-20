#!/usr/bin/env node
// === bake_npm_licenses.mjs ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// ======================================================================================================================
//
// Usage: bake_npm_licenses.mjs <pkg-source-dir> <out-dir> [--esbuild-metafile=<path>]
//
// Writes <out-dir>/<package-name>/LICENSE for every prod dep of <pkg-source-dir>. When the
// optional esbuild metafile is supplied, narrows to packages whose sources reached the bundle
// (drops prod deps that only exist as peer-dep transitives and never ship).

import { createRequire } from "node:module";
import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve as resolvePath } from "node:path";

const positional = [];
let esbuildMetafile = null;
for (const arg of process.argv.slice(2)) {
  if (arg.startsWith("--esbuild-metafile=")) {
    esbuildMetafile = resolvePath(arg.slice("--esbuild-metafile=".length));
  } else {
    positional.push(arg);
  }
}
const [pkgDirArg, outDirArg] = positional;
if (!pkgDirArg || !outDirArg) {
  console.error("Usage: bake_npm_licenses.mjs <pkg-source-dir> <out-dir> [--esbuild-metafile=<path>]");
  process.exit(2);
}

const pkgDir = resolvePath(pkgDirArg);
const outDir = resolvePath(outDirArg);

let bundledNames = null;
if (esbuildMetafile) {
  const meta = JSON.parse(readFileSync(esbuildMetafile, "utf8"));
  bundledNames = new Set();
  for (const inputPath of Object.keys(meta.inputs ?? {})) {
    const m = inputPath.match(/(?:^|\/)node_modules\/((?:@[^/]+\/)?[^/]+)/);
    if (m) bundledNames.add(m[1]);
  }
}

const requireFromPkg = createRequire(join(pkgDir, "package.json"));
const licenseChecker = requireFromPkg("license-checker-rseidelsohn");

await new Promise((resolveP, rejectP) => {
  licenseChecker.init(
    { start: pkgDir, production: true, excludePrivatePackages: true, direct: false },
    (err, packages) => {
      if (err) {
        rejectP(err);
        return;
      }
      rmSync(outDir, { recursive: true, force: true });
      mkdirSync(outDir, { recursive: true });
      let count = 0;
      let skipped = 0;
      for (const [identifier, info] of Object.entries(packages)) {
        const lastAt = identifier.lastIndexOf("@");
        const namePart = lastAt > 0 ? identifier.slice(0, lastAt) : identifier;
        if (bundledNames !== null && !bundledNames.has(namePart)) {
          skipped += 1;
          continue;
        }
        const safeName = namePart.replace(/\//g, "-");
        const dest = join(outDir, safeName);
        mkdirSync(dest, { recursive: true });
        if (info.licenseFile) {
          copyFileSync(info.licenseFile, join(dest, "LICENSE"));
        } else {
          writeFileSync(
            join(dest, "LICENSE"),
            `License: ${info.licenses ?? "UNKNOWN"}\n(no LICENSE file present in package)\n`,
          );
        }
        count += 1;
      }
      const note = bundledNames !== null ? ` (skipped ${skipped} unbundled)` : "";
      console.log(`bake_npm_licenses: emitted ${count} licenses to ${outDir}${note}`);
      resolveP();
    },
  );
});
