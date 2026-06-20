// === reconnect.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Verifies the disconnect / reconnect / reestablishAll round-trip end to end: take down the
// server, watch the client notice, bring the server back up, watch the client reconnect, and
// confirm that re-declared interests resolve to live objects again.
//
// Owns its own Sen subprocess on a separate port from the suite-wide Sen so the bounce here
// does not affect other tests. SEN_BINARY comes from CMake (same as globalSetup); the YAML
// fixture is resolved relative to this file.

import { describe, it, expect, vi } from "vitest";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { connect } from "../../src/index.js";
import type { Client } from "../../src/index.js";
import { spawnSen, waitForPortClosed } from "./subprocess.js";

const reconnectPort = 8081;
const here = dirname(fileURLToPath(import.meta.url));
const reconnectConfig = join(here, "configs", "reconnect.yaml");

function requiredEnv(name: string): string {
  const v = process.env[name];
  if (!v) throw new Error(`${name} is not set; set it before running reconnect.test.ts.`);
  return v;
}

describe("reconnect + reestablishAll", () => {
  it("re-resolves interests after the server is bounced", async () => {
    const senBinary = requiredEnv("SEN_BINARY");

    let sen = await spawnSen({ binary: senBinary, config: reconnectConfig, port: reconnectPort });

    // Reconnect enabled and aggressive: loopback respawn is sub-second, so tight delays keep the
    // test from waiting on the default backoff. The wait windows below are generous on purpose:
    // sanitizer lanes boot the server several times slower, and waitFor returns early on success.
    const client: Client = await connect({
      url: `ws://127.0.0.1:${reconnectPort}`,
      defaultTimeoutMs: 2_000,
      openTimeoutMs: 3_000,
      reconnect: { enabled: true, initialDelayMs: 100, maxDelayMs: 500, factor: 2 },
      onError: () => undefined,
    });

    try {
      let disconnects = 0;
      let reconnects = 0;
      const cancelDisc = client.onDisconnect(() => {
        disconnects += 1;
      });
      const cancelReco = client.onReconnect(() => {
        reconnects += 1;
      });

      const interest = await client.declareInterest({
        name: "reconnect_myclass",
        query: "SELECT my_package.MyClass FROM test.primary",
      });
      await vi.waitFor(() => expect(interest.objects().length).toBeGreaterThanOrEqual(1));

      // Register both lifecycle handlers BEFORE the bounce so we can check the contract:
      // every onObjectAdded across a reestablish is matched by a prior onObjectRemoved.
      const addedNames: string[] = [];
      const removedNames: string[] = [];
      interest.onObjectAdded((obj) => addedNames.push(obj.name));
      interest.onObjectRemoved((name) => removedNames.push(name));
      // onObjectAdded replays for current matches, so we expect one initial add here.
      const initialAdds = addedNames.length;
      expect(initialAdds).toBeGreaterThanOrEqual(1);

      await sen.stop();
      await waitForPortClosed(reconnectPort, 15_000);
      await vi.waitFor(() => expect(disconnects).toBeGreaterThanOrEqual(1), { timeout: 15_000 });

      sen = await spawnSen({ binary: senBinary, config: reconnectConfig, port: reconnectPort });
      await vi.waitFor(() => expect(reconnects).toBeGreaterThanOrEqual(1), { timeout: 30_000 });

      // reestablishAll re-declares the interest against the fresh Sen; the resulting object set
      // should look the same as before the bounce. This manual call deliberately races the
      // autoReestablish pass the client fires on its own - single-flight must coalesce them
      // (two interleaved passes used to strand the handle empty on "interest already exists").
      await client.reestablishAll();
      await vi.waitFor(() => expect(interest.objects().length).toBeGreaterThanOrEqual(1), {
        timeout: 15_000,
      });
      expect(interest.objects()[0]?.className).toBe("my_package.MyClass");

      // Contract: each previously-matched object fired onObjectRemoved when reestablish ran,
      // then onObjectAdded again when the new interestUpdate landed. The pairs match by name.
      expect(removedNames).toEqual(["instance1"]);
      expect(addedNames.length).toBe(initialAdds + 1);
      expect(addedNames[addedNames.length - 1]).toBe("instance1");

      cancelDisc();
      cancelReco();
    } finally {
      client.close();
      await sen.stop();
    }
  }, 90_000);
});
