// === sanitize_env.test.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { sanitizeEnv } from "../../src/recording_runner.js";

describe("sanitizeEnv", () => {
  it("strips credentials and other unlisted vars", () => {
    const out = sanitizeEnv({
      ANTHROPIC_API_KEY: "secret",
      AWS_SECRET_ACCESS_KEY: "secret",
      GH_TOKEN: "secret",
      OPENAI_API_KEY: "secret",
      HOME: "/home/user",
      PATH: "/usr/bin",
    });
    expect(out).toEqual({ HOME: "/home/user", PATH: "/usr/bin" });
  });

  it("keeps every allowed var when present", () => {
    const out = sanitizeEnv({
      PATH: "/usr/bin",
      PYTHONPATH: "/opt/py",
      HOME: "/home/user",
      LANG: "en_US.UTF-8",
      LC_ALL: "C",
      TZ: "UTC",
      TMPDIR: "/tmp",
      RANDOM_OTHER: "drop me",
    });
    expect(out).toEqual({
      PATH: "/usr/bin",
      PYTHONPATH: "/opt/py",
      HOME: "/home/user",
      LANG: "en_US.UTF-8",
      LC_ALL: "C",
      TZ: "UTC",
      TMPDIR: "/tmp",
    });
  });

  it("omits keys whose value is undefined", () => {
    // Missing allowed key must drop, not surface as an empty string.
    const out = sanitizeEnv({ PATH: "/usr/bin" });
    expect(Object.keys(out)).toEqual(["PATH"]);
  });
});
