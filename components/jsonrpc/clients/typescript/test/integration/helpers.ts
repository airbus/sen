// === helpers.ts ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { connect, type Client, type InterestHandle, type ObjectHandle } from "../../src/index.js";

export const senPort = Number(process.env.SEN_PORT ?? 8080);
export const senUrl = `ws://127.0.0.1:${senPort}`;

/** Open a connection to the Sen subprocess spawned by globalSetup. Throws on timeout. */
export async function openClient(): Promise<Client> {
  return connect({
    url: senUrl,
    // Tight timeouts: loopback round-trips are sub-ms; surface test failures fast rather than
    // hanging the suite. Reconnect is off for the same reason.
    defaultTimeoutMs: 2_000,
    openTimeoutMs: 3_000,
    reconnect: { enabled: false },
    // Suppress the default console.error so a single intentional failure does not pollute the
    // test output. Per-test code can override if needed.
    onError: () => undefined,
  });
}

/**
 * Resolve with the first object the interest matches, whether the match was already in the
 * initial server push or arrives later. Uses the idiomatic `onObjectAdded` callback (which
 * replays for already-present objects), so it avoids the race a synchronous
 * `interest.objects()[0]` would have when the createInterest response and the initial
 * interestUpdate notification arrive out of order. Rejects with a clear message after
 * `timeoutMs`, instead of letting the surrounding test timeout swallow the cause.
 */
export function firstMatch(interest: InterestHandle, timeoutMs = 5_000): Promise<ObjectHandle> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(
        new Error(
          `firstMatch: no object arrived within ${timeoutMs}ms (interest=${interest.name}, ` +
            `state=${interest.state}, objects=${interest.objects().length})`,
        ),
      );
    }, timeoutMs);
    interest.onObjectAdded((obj) => {
      clearTimeout(timer);
      resolve(obj);
    });
  });
}
