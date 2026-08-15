// === use_interest_validation.ts ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useRef, useState } from "react";

import type { Client } from "@sen/client";

// Runs name checks then a throwaway declareInterest probe so backend rejections
// (unknown class, malformed WHERE) surface inline before the real save.
export interface InterestValidation {
  error: string | null;
  submitting: boolean;
  validate: (
    name: string,
    composedQuery: string,
    onSuccess: (name: string, query: string) => void,
  ) => Promise<void>;
  abort: () => void;
}

const NAME_PATTERN = /^[A-Za-z0-9_-]+$/;

export function useInterestValidation({
  client,
  existingNames,
}: {
  client: Client;
  existingNames: string[];
}): InterestValidation {
  const [error, setError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);
  // Bumped on validate() and abort(); a stale gen lets the probe closure self-cancel.
  const generationRef = useRef(0);
  // Unmount also bumps so a probe resolving post-teardown self-cancels.
  useEffect(
    () => () => {
      generationRef.current++;
    },
    [],
  );

  const abort = useCallback(() => {
    generationRef.current++;
    setSubmitting(false);
  }, []);

  const validate = useCallback(
    async (name: string, composedQuery: string, onSuccess: (name: string, query: string) => void) => {
      setError(null);
      const gen = ++generationRef.current;
      const trimmedName = name.trim();
      if (!trimmedName) {
        setError("Name is required.");
        return;
      }
      if (!NAME_PATTERN.test(trimmedName)) {
        setError("Name may only contain letters, digits, underscore, and hyphen.");
        return;
      }
      if (existingNames.includes(trimmedName)) {
        setError(`A query named "${trimmedName}" already exists on this bus.`);
        return;
      }
      if (!composedQuery) {
        setError("Query is empty.");
        return;
      }
      setSubmitting(true);
      try {
        const probeName = `__validate_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
        const handle = await client.declareInterest({ name: probeName, query: composedQuery });
        await handle.release();
        if (gen !== generationRef.current) return;
        onSuccess(trimmedName, composedQuery);
      } catch (err) {
        if (gen !== generationRef.current) return;
        setError(err instanceof Error ? err.message : String(err));
      } finally {
        if (gen === generationRef.current) setSubmitting(false);
      }
    },
    [client, existingNames],
  );

  return { error, submitting, validate, abort };
}
