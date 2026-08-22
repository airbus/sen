// === bounded_capture.test.ts =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, expect, it } from "vitest";
import { BoundedCapture } from "../../src/recording_runner.js";

const SENTINEL = "\n... (output truncated)\n";
const SENTINEL_LEN = Buffer.byteLength(SENTINEL, "utf8");

describe("BoundedCapture", () => {
  it("captures everything when total bytes fit under the cap", () => {
    const cap = new BoundedCapture(64);
    cap.push(Buffer.from("hello"));
    cap.push(Buffer.from(" world"));
    expect(cap.truncated).toBe(false);
    expect(cap.toString()).toBe("hello world");
  });

  it("returns the empty string when nothing was pushed", () => {
    const cap = new BoundedCapture(8);
    expect(cap.truncated).toBe(false);
    expect(cap.toString()).toBe("");
  });

  it("truncates and appends the sentinel within the cap", () => {
    const total = 50;
    const contentCap = total - SENTINEL_LEN;
    const cap = new BoundedCapture(total);
    cap.push(Buffer.from("a".repeat(contentCap + 10)));
    expect(cap.truncated).toBe(true);
    const text = cap.toString();
    expect(text.endsWith(SENTINEL)).toBe(true);
    expect(Buffer.byteLength(text, "utf8")).toBe(total);
  });

  it("truncates without partial-chunk slice when the content cap is hit on a chunk boundary", () => {
    const total = 50;
    const contentCap = total - SENTINEL_LEN;
    const cap = new BoundedCapture(total);
    cap.push(Buffer.from("a".repeat(contentCap)));
    cap.push(Buffer.from("more"));
    expect(cap.truncated).toBe(true);
    expect(cap.toString()).toBe("a".repeat(contentCap) + SENTINEL);
  });

  it("drops further chunks after the sentinel has been appended", () => {
    const cap = new BoundedCapture(50);
    cap.push(Buffer.from("a".repeat(100)));
    expect(cap.truncated).toBe(true);
    const before = cap.toString();
    cap.push(Buffer.from("more"));
    expect(cap.toString()).toBe(before);
  });

  it("with cap below the sentinel length, truncates with empty output", () => {
    const cap = new BoundedCapture(0);
    cap.push(Buffer.from("x"));
    expect(cap.truncated).toBe(true);
    expect(cap.toString()).toBe("");
  });
});
