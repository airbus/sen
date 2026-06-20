// === set.test.ts =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeAll, afterAll, vi } from "vitest";
import { firstMatch, openClient } from "./helpers.js";
import { JsonRpcError } from "../../src/index.js";
import type { Client } from "../../src/index.js";

describe("setProperty against my_package.MyClass", () => {
  let client: Client;
  beforeAll(async () => {
    client = await openClient();
  });
  afterAll(() => {
    client.close();
  });

  // prop7 is `i32 [writable]` and is not modified by MyClassImpl::update, so the value we
  // write stays put long enough for the subscription notification to fire. The impl's
  // `prop7AcceptsSet` callback accepts values in [-5, 5]; we pick 3 to stay in-range.
  it("writes prop7 (i32) and observes the new value via subscription", async () => {
    const interest = await client.declareInterest({
      name: "myclass_for_set",
      query: "SELECT my_package.MyClass FROM test.primary",
    });
    try {
      const obj = await firstMatch(interest);
      const seen: number[] = [];
      const cancel = obj.onPropertyChanged("prop7", (value) => {
        seen.push(value as number);
      });
      await vi.waitFor(() => expect(seen.length).toBeGreaterThanOrEqual(1));
      await obj.set("prop7", 3);
      await vi.waitFor(() => expect(seen[seen.length - 1]).toBe(3));
      cancel();
    } finally {
      await interest.release();
    }
  });

  it("rejects writes to a read-only property with JsonRpcError", async () => {
    const interest = await client.declareInterest({
      name: "myclass_ro_check",
      query: "SELECT my_package.MyClass FROM test.primary",
    });
    try {
      const obj = await firstMatch(interest);
      // `prop1` is declared `[static]`; the server should refuse runtime writes.
      await expect(obj.set("prop1", "nope")).rejects.toBeInstanceOf(JsonRpcError);
    } finally {
      await interest.release();
    }
  });
});
