// === env_flags.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { EnvFlagError, isEnvFlagSet } from "../../src/util/env_flags.js";

const FLAG = "SEN_MCP_GATEWAY_READONLY";

describe("isEnvFlagSet", () => {
  it("is false when the variable is absent", () => {
    expect(isEnvFlagSet(FLAG, {})).toBe(false);
  });

  it("accepts every documented truthy spelling, in any case", () => {
    for (const v of ["1", "true", "TRUE", "True", "yes", "YES", "on", "ON"]) {
      expect(isEnvFlagSet(FLAG, { [FLAG]: v })).toBe(true);
    }
  });

  it("accepts every documented falsey spelling, in any case", () => {
    for (const v of ["0", "false", "FALSE", "no", "NO", "off", "OFF"]) {
      expect(isEnvFlagSet(FLAG, { [FLAG]: v })).toBe(false);
    }
  });

  it("reads an empty value as unset, matching envOrUndefined", () => {
    expect(isEnvFlagSet(FLAG, { [FLAG]: "" })).toBe(false);
    expect(isEnvFlagSet(FLAG, { [FLAG]: "   " })).toBe(false);
  });

  it("ignores surrounding whitespace rather than failing on a shell accident", () => {
    expect(isEnvFlagSet(FLAG, { [FLAG]: "true " })).toBe(true);
    expect(isEnvFlagSet(FLAG, { [FLAG]: " 1" })).toBe(true);
    expect(isEnvFlagSet(FLAG, { [FLAG]: " off " })).toBe(false);
  });

  // The point of the whole module: these used to return false, silently leaving the write
  // tools and the python child enabled on a gateway its operator believed was read-only.
  it("throws on a value it does not recognise, rather than defaulting to permissive", () => {
    for (const v of ["enabled", "Y", "y", "t", "2", "readonly", "please"]) {
      expect(() => isEnvFlagSet(FLAG, { [FLAG]: v })).toThrow(EnvFlagError);
    }
  });

  it("names the variable and both accepted sets in the error", () => {
    let message = "";
    try {
      isEnvFlagSet(FLAG, { [FLAG]: "enabled" });
    } catch (err) {
      message = (err as Error).message;
    }
    expect(message).toContain(FLAG);
    expect(message).toContain("true");
    expect(message).toContain("false");
    expect(message).toContain('"enabled"');
  });
});
