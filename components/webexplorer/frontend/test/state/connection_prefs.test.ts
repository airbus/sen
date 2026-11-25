// === connection_prefs.test.ts ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";

import { normalizeWsUrl } from "../../src/state/use_connection.js";

describe("normalizeWsUrl", () => {
  it("preserves explicit ws:// and wss:// schemes", () => {
    expect(normalizeWsUrl("ws://127.0.0.1:8080")).toBe("ws://127.0.0.1:8080");
    expect(normalizeWsUrl("wss://sen.local:8443/ws")).toBe("wss://sen.local:8443/ws");
    expect(normalizeWsUrl("  WS://127.0.0.1:8081  ")).toBe("ws://127.0.0.1:8081");
  });

  it("converts http:// and https:// URLs to ws:// and wss://", () => {
    expect(normalizeWsUrl("http://127.0.0.1:8081")).toBe("ws://127.0.0.1:8081");
    expect(normalizeWsUrl("https://sen.local:8443")).toBe("wss://sen.local:8443");
  });

  it("prefixes bare host:port inputs based on page protocol", () => {
    expect(normalizeWsUrl("127.0.0.1:8081", "http:")).toBe("ws://127.0.0.1:8081");
    expect(normalizeWsUrl("localhost:8081", "https:")).toBe("wss://localhost:8081");
    expect(normalizeWsUrl("[::1]:8081", "http:")).toBe("ws://[::1]:8081");
  });

  it("returns empty string for blank inputs", () => {
    expect(normalizeWsUrl("")).toBe("");
    expect(normalizeWsUrl("   ")).toBe("");
  });
});
