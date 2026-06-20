// === use_set_property.ts =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useState } from "react";

import type { CallOptions, ObjectHandle } from "../index.js";

/** State of the most recent setProperty call. `idle` until the first call. */
export type SetPropertyStatus = "idle" | "pending" | "ok" | "error";

export interface UseSetPropertyResult {
  setProperty: (name: string, value: unknown, opts?: CallOptions) => Promise<void>;
  status: SetPropertyStatus;
  error: Error | null;
}

/**
 * Wraps `obj.set()` with reactive pending/error state suitable for an Apply button. The
 * library validates the value against the property's wire type and rejects with
 * {@link JsonRpcError} for non-writable properties; both surface here as `status: "error"`
 * with the original `error` set.
 */
export function useSetProperty(obj: ObjectHandle | null): UseSetPropertyResult {
  const [status, setStatus] = useState<SetPropertyStatus>("idle");
  const [error, setError] = useState<Error | null>(null);

  const setProperty = useCallback(
    async (name: string, value: unknown, opts?: CallOptions): Promise<void> => {
      if (!obj) {
        const err = new Error("useSetProperty: object handle is null");
        setStatus("error");
        setError(err);
        throw err;
      }
      setStatus("pending");
      setError(null);
      try {
        await obj.set(name, value, opts ?? {});
        setStatus("ok");
      } catch (err) {
        const e = err instanceof Error ? err : new Error(String(err));
        setStatus("error");
        setError(e);
        throw e;
      }
    },
    [obj],
  );

  return { setProperty, status, error };
}
