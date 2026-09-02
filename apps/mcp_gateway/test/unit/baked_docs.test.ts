// === baked_docs.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { BAKED_DOCS, BAKED_DOCS_BY_URI } from "../../src/baked_docs.js";

// Snapshot of the doc surface the LLM sees on connect: a new mkdocs page must not ship
// silently. Update this list and scripts/bake_docs.mjs together.
const EXPECTED_URIS: readonly string[] = [
  "sen://docs/components/ether",
  "sen://docs/components/explorer",
  "sen://docs/components/index",
  "sen://docs/components/influx",
  "sen://docs/components/jsonrpc",
  "sen://docs/components/jsonrpc-ts-client",
  "sen://docs/components/logmaster",
  "sen://docs/components/mcp-gateway",
  "sen://docs/components/py",
  "sen://docs/components/recording",
  "sen://docs/components/replaying",
  "sen://docs/components/shell",
  "sen://docs/components/tracy",
  "sen://docs/components/webexplorer",
  "sen://docs/howto/consuming-interfaces",
  "sen://docs/howto/objects",
  "sen://docs/howto/troubleshooting",
  "sen://docs/users-guide/compatibility-conversions",
  "sen://docs/users-guide/configuration",
  "sen://docs/users-guide/db-library",
  "sen://docs/users-guide/db-python-bindings",
  "sen://docs/users-guide/execution-model",
  "sen://docs/users-guide/faq",
  "sen://docs/users-guide/glossary",
  "sen://docs/users-guide/hla",
  "sen://docs/users-guide/main-concepts",
  "sen://docs/users-guide/mental-model",
  "sen://docs/users-guide/sql",
  "sen://docs/users-guide/stl",
  "sen://docs/users-guide/stl-grammar",
  "sen://docs/users-guide/threading",
  "sen://docs/users-guide/versioning",
  "sen://docs/users-guide/workflow",
];

describe("baked_docs", () => {
  it("matches the curated URI manifest exactly", () => {
    const actual = BAKED_DOCS.map((d) => d.uri).sort();
    expect(actual).toEqual([...EXPECTED_URIS].sort());
  });

  it("indexes every doc by URI", () => {
    expect(BAKED_DOCS_BY_URI.size).toBe(BAKED_DOCS.length);
    for (const doc of BAKED_DOCS) {
      expect(BAKED_DOCS_BY_URI.get(doc.uri)).toBe(doc);
    }
  });

  it("publishes the db-python-bindings reference (used by getRecordingDocs)", () => {
    expect(BAKED_DOCS_BY_URI.has("sen://docs/users-guide/db-python-bindings")).toBe(true);
  });
});
