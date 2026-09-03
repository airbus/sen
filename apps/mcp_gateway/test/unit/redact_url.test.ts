// === redact_url.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { redactUrlForAudit } from "../../src/util/redact_url.js";

describe("redactUrlForAudit", () => {
  it("passes an ordinary kernel URL through unchanged", () => {
    expect(redactUrlForAudit("ws://127.0.0.1:8080")).toBe("ws://127.0.0.1:8080");
    expect(redactUrlForAudit("wss://sim.example:9443")).toBe("wss://sim.example:9443");
  });

  it("keeps a real path, drops a bare slash", () => {
    expect(redactUrlForAudit("ws://host:8080/")).toBe("ws://host:8080");
    expect(redactUrlForAudit("ws://host:8080/kernel")).toBe("ws://host:8080/kernel");
  });

  // The two places a browser client can put a credential, since the WebSocket API cannot set
  // request headers and the server reads its credential from one.
  it("drops userinfo", () => {
    expect(redactUrlForAudit("ws://user:hunter2@host:8080")).toBe("ws://host:8080 (redacted)");
    expect(redactUrlForAudit("ws://tokenonly@host:8080")).toBe("ws://host:8080 (redacted)");
  });

  it("drops the query string and fragment", () => {
    expect(redactUrlForAudit("ws://host:8080?token=abc123")).toBe("ws://host:8080 (redacted)");
    expect(redactUrlForAudit("ws://host:8080/k?access_token=x")).toBe("ws://host:8080/k (redacted)");
    expect(redactUrlForAudit("ws://host:8080#secret")).toBe("ws://host:8080 (redacted)");
  });

  it("marks removal so a reader can tell a bare URL from a trimmed one", () => {
    expect(redactUrlForAudit("ws://host:8080")).not.toContain("redacted");
    expect(redactUrlForAudit("ws://host:8080?a=1")).toContain("redacted");
  });

  // Withheld rather than passed through: an unparseable value cannot be shown to be free of a
  // credential, and a typo'd host with a real password is exactly the shape that fails to parse.
  it("withholds a value it cannot parse", () => {
    expect(redactUrlForAudit("not a url")).toBe("<unparseable url>");
    expect(redactUrlForAudit("")).toBe("<unparseable url>");
    expect(redactUrlForAudit("ws://user:hunter2@")).toBe("<unparseable url>");
  });

  it("never returns a string containing the password it was given", () => {
    for (const raw of [
      "ws://user:hunter2@host:8080",
      "ws://host:8080?token=hunter2",
      "ws://host:8080/path?a=1&secret=hunter2#hunter2",
    ]) {
      expect(redactUrlForAudit(raw)).not.toContain("hunter2");
    }
  });
});
