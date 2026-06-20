// === events.test.ts ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect, beforeAll, afterAll, vi } from "vitest";
import { firstMatch, openClient } from "./helpers.js";
import type { Client, Var } from "../../src/index.js";

describe("event subscriptions against inheritance.MySubClass", () => {
  let client: Client;
  beforeAll(async () => {
    client = await openClient();
  });
  afterAll(() => {
    client.close();
  });

  // doSomethingElse(arg) fires somethingElseHappened(arg) synchronously on the server side. We
  // pre-subscribe to the event in the interest declaration (atomic with interest creation, so
  // the wire subscription is up before any invoke fires) and assert the event lands with the
  // same arg after invoking doSomethingElse.
  it("delivers somethingElseHappened(arg) after invoking doSomethingElse(arg)", async () => {
    const interest = await client.declareInterest({
      name: "subclass_for_events",
      query: "SELECT inheritance.MySubClass FROM test.secondary",
      subscribe: { events: ["somethingElseHappened"] },
    });
    try {
      const obj = await firstMatch(interest);
      const seen: Var[][] = [];
      const cancel = obj.onEventTriggered("somethingElseHappened", (args) => {
        seen.push(args);
      });
      await obj.invoke("doSomethingElse", { arg: 7 });
      await vi.waitFor(() => expect(seen.length).toBeGreaterThanOrEqual(1), { timeout: 5_000 });
      const [arg] = seen[0]!;
      expect(arg).toBe(7);
      cancel();
    } finally {
      await interest.release();
    }
  });
});
