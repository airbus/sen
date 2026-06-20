// === subscribe_codec.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Translates the friendly Layer-3 subscribe shape into the wire-level `MaybeSubscribeBlock`.
// One-way: the wire form is built on the way out; nothing parses it back. Boundary code so
// callers can write `subscribe: { properties: "*" }` instead of constructing the variant tag.

import type { MaybeMemberSelector, MaybeSubscribeBlock } from "../generated/index.js";
import type { PreSubscription } from "../handles.js";

export function toWireSubscribeBlock(
  subscribe: PreSubscription | undefined,
): MaybeSubscribeBlock {
  if (subscribe === undefined) return null;
  return {
    properties: toMemberSelector(subscribe.properties),
    events: toMemberSelector(subscribe.events),
    maxRateHz: subscribe.maxRateHz ?? null,
  };
}

function toMemberSelector(selector: "*" | string[] | undefined): MaybeMemberSelector {
  if (selector === undefined) return null;
  if (selector === "*") {
    return { type: "sen.components.jsonrpc.WildcardSelection", value: {} };
  }
  return {
    type: "sen.components.jsonrpc.NamedSelection",
    value: { memberNames: selector },
  };
}
