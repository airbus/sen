// === use_invoke.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useState } from "react";

import type { CallOptions, ObjectHandle, Var } from "../index.js";

/** State of the most recent invoke call. `idle` until the first call. */
export type InvokeStatus = "idle" | "pending" | "ok" | "error";

export interface UseInvokeResult {
  invoke: (method: string, args?: Record<string, unknown>, opts?: CallOptions) => Promise<Var>;
  /** Return `status` to `idle` and clear `result` / `error`. For dismissing a sticky error
   *  on an Invoke button without issuing a new call. No-op when status is already `idle`. */
  reset: () => void;
  status: InvokeStatus;
  result: Var | undefined;
  error: Error | null;
}

/**
 * Wraps `obj.invoke()` with reactive pending/result/error state suitable for an Invoke
 * button in a method form. The library validates argument names against the method's
 * declared signature before the call goes on the wire; that and any backend rejection both
 * surface here as `status: "error"` with the original `error` set.
 */
export function useInvoke(obj: ObjectHandle | null): UseInvokeResult {
  const [status, setStatus] = useState<InvokeStatus>("idle");
  const [result, setResult] = useState<Var | undefined>(undefined);
  const [error, setError] = useState<Error | null>(null);

  const invoke = useCallback(
    async (method: string, args?: Record<string, unknown>, opts?: CallOptions): Promise<Var> => {
      if (!obj) {
        const err = new Error("useInvoke: object handle is null");
        setStatus("error");
        setError(err);
        throw err;
      }
      setStatus("pending");
      setError(null);
      setResult(undefined);
      try {
        const r = await obj.invoke(method, args ?? {}, opts ?? {});
        setResult(r);
        setStatus("ok");
        return r;
      } catch (err) {
        const e = err instanceof Error ? err : new Error(String(err));
        setStatus("error");
        setError(e);
        throw e;
      }
    },
    [obj],
  );

  const reset = useCallback(() => {
    setStatus("idle");
    setResult(undefined);
    setError(null);
  }, []);

  return { invoke, reset, status, result, error };
}
