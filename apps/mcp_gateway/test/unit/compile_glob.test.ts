// === compile_glob.test.ts ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { GatewayInputError } from "../../src/errors.js";
import { compileGlob, MAX_GLOB_LENGTH } from "../../src/tools.js";

describe("compileGlob", () => {
  it("matches a literal pattern only when the input is exactly equal", () => {
    const m = compileGlob("foo.txt");
    expect(m.test("foo.txt")).toBe(true);
    expect(m.test("Xfoo.txt")).toBe(false);
    expect(m.test("foo.txt.bak")).toBe(false);
  });

  it("treats * as zero-or-more characters", () => {
    const m = compileGlob("school_*");
    expect(m.test("school_")).toBe(true);
    expect(m.test("school_a")).toBe(true);
    expect(m.test("school_long_name")).toBe(true);
    expect(m.test("school")).toBe(false);
    expect(m.test("preschool_x")).toBe(false);
  });

  it("treats ? as exactly one character", () => {
    const m = compileGlob("a?b");
    expect(m.test("axb")).toBe(true);
    expect(m.test("a.b")).toBe(true);
    expect(m.test("ab")).toBe(false);
    expect(m.test("aXXb")).toBe(false);
  });

  it("treats regex metacharacters as literals", () => {
    const m = compileGlob("a.b+c");
    expect(m.test("a.b+c")).toBe(true);
    expect(m.test("aXb+c")).toBe(false);
    expect(m.test("ab+c")).toBe(false);
  });

  it("anchors the pattern at both ends", () => {
    const m = compileGlob("*.tar.gz");
    expect(m.test("archive.tar.gz")).toBe(true);
    expect(m.test(".tar.gz")).toBe(true);
    expect(m.test("archive.tar.gz.bak")).toBe(false);
    expect(m.test("prefix.archive.tar.gz.bak")).toBe(false);
  });

  it("combines * and ? in one pattern", () => {
    const m = compileGlob("?ec_*.bin");
    expect(m.test("rec_1.bin")).toBe(true);
    expect(m.test("Xec_long_name.bin")).toBe(true);
    expect(m.test("ec_1.bin")).toBe(false);
    expect(m.test("rec_1.bin.bak")).toBe(false);
  });

  it("treats [ ] ( ) as literal characters (no fnmatch char-class support)", () => {
    const m = compileGlob("a[b](c)");
    expect(m.test("a[b](c)")).toBe(true);
    expect(m.test("ab")).toBe(false);
    expect(m.test("ac")).toBe(false);
  });

  it("collapses consecutive and trailing stars", () => {
    expect(compileGlob("**").test("")).toBe(true);
    expect(compileGlob("a**b").test("ab")).toBe(true);
    expect(compileGlob("a**b").test("axyzb")).toBe(true);
    expect(compileGlob("a*").test("a")).toBe(true);
    expect(compileGlob("*").test("anything")).toBe(true);
    expect(compileGlob("").test("")).toBe(true);
    expect(compileGlob("").test("x")).toBe(false);
  });

  it("backtracks the last star far enough to match a repeated suffix", () => {
    expect(compileGlob("*ab*cd").test("xxabyyabzzcd")).toBe(true);
    expect(compileGlob("*ab*cd").test("xxabyycdzz")).toBe(false);
    expect(compileGlob("a*b?c").test("aXXXbYc")).toBe(true);
    expect(compileGlob("a*b?c").test("aXXXbc")).toBe(false);
  });

  // The RegExp this replaced needed 115 seconds on exactly this input, stalling the whole
  // single-threaded gateway.
  it("rejects a pathological star pattern in bounded time", () => {
    const pattern = `${"*a".repeat(8)}*b`;
    const name = "a".repeat(124);
    const startedAt = performance.now();
    expect(compileGlob(pattern).test(name)).toBe(false);
    expect(performance.now() - startedAt).toBeLessThan(100);
  });

  it("stays bounded on a full-length pattern against a full-length basename", () => {
    const pattern = "*a?".repeat(Math.floor(MAX_GLOB_LENGTH / 3));
    const name = "a".repeat(255);
    const startedAt = performance.now();
    compileGlob(pattern).test(name);
    expect(performance.now() - startedAt).toBeLessThan(100);
  });

  it("refuses a pattern longer than the cap", () => {
    expect(() => compileGlob("*".repeat(MAX_GLOB_LENGTH + 1))).toThrow(GatewayInputError);
    expect(() => compileGlob("*".repeat(MAX_GLOB_LENGTH))).not.toThrow();
  });
});
