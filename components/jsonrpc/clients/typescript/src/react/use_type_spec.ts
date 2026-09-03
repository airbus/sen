// === use_type_spec.ts ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { Client, CustomTypeSpec } from "../index.js";

/**
 * Reactive lookup for a `CustomTypeSpec` by qualified name. Returns the spec if cached now,
 * `undefined` otherwise; re-renders when the spec lands in the cache.
 */
export function useTypeSpec(
  client: Client | null,
  qualifiedName: string | null,
): CustomTypeSpec | undefined {
  const [spec, setSpec] = useState<CustomTypeSpec | undefined>(() =>
    client && qualifiedName ? client.getType(qualifiedName) : undefined,
  );

  useEffect(() => {
    if (!client || !qualifiedName) {
      setSpec(undefined);
      return;
    }
    setSpec(client.getType(qualifiedName));
    return client.onTypeAdded(() => {
      setSpec(client.getType(qualifiedName));
    });
  }, [client, qualifiedName]);

  return spec;
}
