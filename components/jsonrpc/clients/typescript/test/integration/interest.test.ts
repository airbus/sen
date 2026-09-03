// === interest.test.ts ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeAll, afterAll } from "vitest";
import { firstMatch, openClient } from "./helpers.js";
import type { Client } from "../../src/index.js";

describe("declareInterest against inheritance.yaml", () => {
  let client: Client;
  beforeAll(async () => {
    client = await openClient();
  });
  afterAll(() => {
    client?.close();
  });

  it("matches a MyClass on test.primary", async () => {
    const interest = await client.declareInterest({
      name: "myclass_primary",
      query: "SELECT my_package.MyClass FROM test.primary",
    });
    try {
      const obj = await firstMatch(interest);
      expect(interest.state).toBe("active");
      expect(obj.className).toBe("my_package.MyClass");
      expect(obj.name).toBe("instance1");
    } finally {
      await interest.release();
    }
  });

  it("release() drops the interest and stops tracking the match set", async () => {
    const interest = await client.declareInterest({
      name: "release_check",
      query: "SELECT my_package.MyClass FROM test.primary",
    });
    await interest.release();
    expect(interest.state).toBe("released");
    expect(interest.objects().length).toBe(0);
  });
});
