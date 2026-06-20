// === invoke.test.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeAll, afterAll } from "vitest";
import { firstMatch, openClient } from "./helpers.js";
import type { Client } from "../../src/index.js";

describe("invoke against my_package.MyClass", () => {
  let client: Client;
  beforeAll(async () => {
    client = await openClient();
  });
  afterAll(() => {
    client.close();
  });

  it("calls addNumbers(a, b) and returns the sum", async () => {
    const interest = await client.declareInterest({
      name: "myclass_for_invoke",
      query: "SELECT my_package.MyClass FROM test.primary",
    });
    try {
      const obj = await firstMatch(interest);
      const reply = await obj.invoke("addNumbers", { a: 2, b: 3 });
      expect(reply).toBe(5);
    } finally {
      await interest.release();
    }
  });
});
