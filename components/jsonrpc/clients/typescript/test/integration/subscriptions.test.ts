// === subscriptions.test.ts ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeAll, afterAll, vi } from "vitest";
import { firstMatch, openClient } from "./helpers.js";
import type { Client, Var } from "../../src/index.js";

describe("property subscriptions against inheritance.yaml", () => {
  let client: Client;
  beforeAll(async () => {
    client = await openClient();
  });
  afterAll(() => {
    client.close();
  });

  it("delivers updates of prop5 (i32) when subscribing to MyClass", async () => {
    const interest = await client.declareInterest({
      name: "myclass_props",
      query: "SELECT my_package.MyClass FROM test.primary",
    });
    try {
      const obj = await firstMatch(interest);
      const seen: Var[] = [];
      const cancel = obj.onPropertyChanged("prop5", (value) => {
        seen.push(value);
      });
      // prop5 is incremented every tick by MyClassImpl::update, so notifications stream in
      // at the component's freqHz (30). One delivery within the default waitFor window is
      // enough to prove the property-change wire path is alive.
      await vi.waitFor(() => expect(seen.length).toBeGreaterThanOrEqual(1));
      expect(typeof seen[0]).toBe("number");
      cancel();
    } finally {
      await interest.release();
    }
  });
});
