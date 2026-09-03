// === subscribe.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Verifies the friendly `subscribe` shape on `declareInterest`. The codec unit tests
// (test/subscribe_codec.test.ts) cover the translation; these tests exercise the wire
// round-trip so the server actually accepts the produced shape.

import { describe, it, expect, beforeAll, afterAll } from "vitest";
import { firstMatch, openClient } from "./helpers.js";
import type { Client } from "../../src/index.js";

describe("declareInterest({ subscribe }) against my_package.MyClass", () => {
  let client: Client;
  beforeAll(async () => {
    client = await openClient();
  });
  afterAll(() => {
    client?.close();
  });

  it("accepts a wildcard property pre-subscription and serves get() from cache", async () => {
    const interest = await client.declareInterest({
      name: "presub_wildcard",
      query: "SELECT my_package.MyClass FROM test.primary",
      subscribe: { properties: "*" },
    });
    try {
      const obj = await firstMatch(interest);
      // With `properties: "*"`, the server seeds `currentValues` for every property in the
      // initial interestUpdate; `get` should resolve from the local cache without a wire trip.
      const prop1 = await obj.get("prop1");
      expect(typeof prop1).toBe("string");
    } finally {
      await interest.release();
    }
  });

  it("accepts a named property pre-subscription", async () => {
    const interest = await client.declareInterest({
      name: "presub_named",
      query: "SELECT my_package.MyClass FROM test.primary",
      subscribe: { properties: ["prop1"] },
    });
    try {
      const obj = await firstMatch(interest);
      const prop1 = await obj.get("prop1");
      expect(typeof prop1).toBe("string");
    } finally {
      await interest.release();
    }
  });

  it("accepts a named event pre-subscription with maxRateHz", async () => {
    const interest = await client.declareInterest({
      name: "presub_events",
      query: "SELECT inheritance.MySubClass FROM test.secondary",
      subscribe: { events: ["somethingElseHappened"], maxRateHz: 5 },
    });
    try {
      const obj = await firstMatch(interest);
      // Smoke: the object handle should still be usable for an invoke.
      const reply = await obj.invoke("addNumbers", { a: 1, b: 4 });
      expect(reply).toBe(5);
    } finally {
      await interest.release();
    }
  });
});
