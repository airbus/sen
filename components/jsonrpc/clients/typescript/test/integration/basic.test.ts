// === basic.test.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";
import { openClient } from "./helpers.js";

describe("connect + close against a real Sen subprocess", () => {
  it("opens a WebSocket connection and reports state 'open'", async () => {
    const client = await openClient();
    try {
      expect(client.connectionState).toBe("open");
    } finally {
      client.close();
    }
  });

  it("close() makes connectionState 'closed' and is safe to call twice", async () => {
    const client = await openClient();
    client.close();
    expect(client.connectionState).toBe("closed");
    expect(() => client.close()).not.toThrow();
  });
});
