// === resources.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { BAKED_DOCS, BAKED_DOCS_BY_URI } from "./baked_docs.js";

export interface ResourceListEntry {
  uri: string;
  name: string;
  mimeType: string;
}

const MIME_TYPE = "text/markdown";

export function listResources(): ResourceListEntry[] {
  return BAKED_DOCS.map((doc) => ({
    uri: doc.uri,
    name: doc.title,
    mimeType: MIME_TYPE,
  }));
}

// Returns MCP's ReadResourceResult; typed loose because the SDK validates the wire boundary.
export function readResource(uri: string): Record<string, unknown> {
  const doc = BAKED_DOCS_BY_URI.get(uri);
  if (doc === undefined) throw new Error(`unknown resource URI: ${uri}`);
  return {
    contents: [{ uri: doc.uri, mimeType: MIME_TYPE, text: doc.content }],
  };
}
