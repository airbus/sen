#!/usr/bin/env node
// === bake_docs.mjs ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Emits src/baked_docs.ts (MCP resources) and src/baked_package.ts (wire identity) from
// the Sen docs tree and package.json. Runs as the npm `prebuild` hook. Fails loud on any
// missing source; the gateway is part of the Sen repo, no out-of-repo mode.

import { readFileSync, writeFileSync, readdirSync, existsSync } from "node:fs";
import { dirname, join, resolve, basename } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(here, "..", "..", "..");
const docsRoot = join(repoRoot, "docs");
const outputPath = resolve(here, "..", "src", "baked_docs.ts");
const packageJsonPath = resolve(here, "..", "package.json");
const packageOutputPath = resolve(here, "..", "src", "baked_package.ts");

// Per-folder URI prefix + an exclude-set or include-set of basenames.
const sources = [
  {
    folder: "users_guide",
    uriPrefix: "sen://docs/users-guide/",
    mode: "exclude",
    set: new Set([
      "cmake.md",          // build mechanics, not behavior an LLM observes
      "command_line.md",   // run mechanics, same reason
      "index.md",          // table-of-contents only; the other docs already cover the topics
      // C++ library refs are off-topic; db_library / db_python_bindings stay (recording tools).
      "core_library.md",
      "kernel_library.md",
      "gen_library.md",
      "util_library.md",
    ]),
  },
  {
    folder: "components",
    uriPrefix: "sen://docs/components/",
    mode: "exclude",
    set: new Set([
      "rest.md",           // REST API is being deprecated in favour of jsonrpc; exclude
    ]),
  },
  {
    folder: "howto_guides",
    uriPrefix: "sen://docs/howto/",
    mode: "include",
    set: new Set([
      "objects.md",        // registration, subscriptions, events; directly relevant
      "troubleshooting.md",
      "consuming_interfaces.md",
    ]),
  },
];

function requireDir(path) {
  if (!existsSync(path)) {
    throw new Error(
      `bake_docs: required docs directory missing at ${path}. The gateway is part of the Sen ` +
        `repo and reads ../../docs at build time; running it outside that layout is not supported.`,
    );
  }
}

function requireFile(path) {
  if (!existsSync(path)) {
    throw new Error(`bake_docs: required doc file missing at ${path}.`);
  }
}

// Exclusions match on basename, so a renamed doc would quietly start shipping to every
// connected model. Require each entry to still name a real file.
function requireExcludedFile(path) {
  if (!existsSync(path)) {
    throw new Error(
      `bake_docs: excluded doc missing at ${path}. If the page was renamed, rename it in the ` +
        `exclude list too; if it is gone, drop the entry.`,
    );
  }
}

requireDir(docsRoot);

// Collect entries + slug-to-URI map; the sanitizer uses the map to rewrite cross-doc links.
const entries = [];
const slugToUri = new Map();

for (const src of sources) {
  const folderPath = join(docsRoot, src.folder);
  requireDir(folderPath);
  let files;
  if (src.mode === "include") {
    files = [...src.set];
    for (const f of files) requireFile(join(folderPath, f));
  } else {
    for (const f of src.set) requireExcludedFile(join(folderPath, f));
    files = readdirSync(folderPath)
      .filter((f) => f.endsWith(".md") && !src.set.has(f))
      .sort();
  }
  for (const file of files) {
    const slug = basename(file, ".md").replace(/_/g, "-");
    const uri = `${src.uriPrefix}${slug}`;
    entries.push({ folder: src.folder, file, slug, uri });
    // Rewriter matches links by basename, e.g. `[X](sql.md)` or `[X](../components/jsonrpc.md)`.
    slugToUri.set(file, uri);
  }
}

function sanitize(markdown, uri) {
  let out = markdown;
  // Drop image lines (with or without a trailing MkDocs attr block).
  out = out.replace(/^!\[[^\]]*\]\([^)]+\)\{[^}]*\}\s*$/gm, "");
  out = out.replace(/^!\[[^\]]*\]\([^)]+\)\s*$/gm, "");
  // Standalone MkDocs attr lines.
  out = out.replace(/^\{:[^}]*\}\s*$/gm, "");
  // Rewrite cross-doc links by basename; unknown targets are left alone.
  out = out.replace(/\[([^\]]+)\]\(([^)\s#]+\.md)(#[^)]*)?\)/g, (match, label, href, anchor) => {
    const file = basename(href);
    const target = slugToUri.get(file);
    if (target === undefined) return match;
    return `[${label}](${target}${anchor ?? ""})`;
  });
  out = out.replace(/\n{3,}/g, "\n\n");
  return `${out.trim()}\n\n<!-- baked from Sen docs; URI: ${uri} -->\n`;
}

const baked = entries.map(({ folder, file, slug, uri }) => {
  const sourcePath = join(docsRoot, folder, file);
  const content = sanitize(readFileSync(sourcePath, "utf8"), uri);
  const title = deriveTitle(content) ?? slug;
  return { uri, title, content };
});

function deriveTitle(content) {
  const m = content.match(/^#\s+(.+?)\s*$/m);
  return m ? m[1].trim() : undefined;
}

function tsLiteral(s) {
  return JSON.stringify(s);
}

const header = `// === baked_docs.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================
// AUTO-GENERATED by scripts/bake_docs.mjs; do not edit by hand. Sources live under docs/.

export interface BakedDoc {
  readonly uri: string;
  readonly title: string;
  readonly content: string;
}

export const BAKED_DOCS: ReadonlyArray<BakedDoc> = [
`;

const body = baked
  .map((d) => `  { uri: ${tsLiteral(d.uri)}, title: ${tsLiteral(d.title)}, content: ${tsLiteral(d.content)} },`)
  .join("\n");

const footer = `
];

export const BAKED_DOCS_BY_URI: ReadonlyMap<string, BakedDoc> = new Map(
  BAKED_DOCS.map((d) => [d.uri, d] as const),
);
`;

writeFileSync(outputPath, header + body + footer, "utf8");

process.stdout.write(`bake_docs: emitted ${entries.length} docs to ${outputPath}\n`);

// Bake the wire identity. SEN_MCP_VERSION (set by CMake from CMAKE_PROJECT_VERSION) takes
// precedence; standalone npm runs fall back to package.json's "0.0.0", marking a dev build.
let version = process.env["SEN_MCP_VERSION"];
if (version === undefined || version === "") {
  const pkg = JSON.parse(readFileSync(packageJsonPath, "utf8"));
  if (typeof pkg.version !== "string") {
    throw new Error("bake_docs: package.json missing version");
  }
  version = pkg.version;
}
// Hyphenated form is the MCP wire identity; the npm scoped name isn't a stable single-token id.
const wireName = "sen-mcp-gateway";
const packageModule =
  `// === baked_package.ts ================================================================================================\n` +
  `//                                               Sen Infrastructure\n` +
  `//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).\n` +
  `//                                    See the LICENSE.txt file for more information.\n` +
  `//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.\n` +
  `// =====================================================================================================================\n` +
  `// AUTO-GENERATED by scripts/bake_docs.mjs; do not edit by hand. Source: package.json.\n\n` +
  `export const PACKAGE_NAME = ${tsLiteral(wireName)};\n` +
  `export const PACKAGE_VERSION = ${tsLiteral(version)};\n`;
writeFileSync(packageOutputPath, packageModule, "utf8");
process.stdout.write(`bake_docs: emitted package identity to ${packageOutputPath}\n`);
