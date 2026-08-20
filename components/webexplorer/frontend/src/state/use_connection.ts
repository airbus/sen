// === use_connection.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useRef, useState } from "react";

import type { Client } from "@sen/client";
import { connect } from "@sen/client";

// WebSocket lifecycle for the App tree. Three races to handle:
//  - StrictMode double-mount: refs persist; mountedRef re-asserted, real unmount closes.
//  - Connect-then-disconnect: connectGenRef bump makes a late connect resolve into a
//    self-close instead of installing an unowned client.
//  - Rapid URL switches: same generation guard - last connect wins.
// handleConnect/handleDisconnect are identity-stable (state via refs).
export function useConnection(defaultUrl: () => string): {
  client: Client | null;
  url: string;
  error: Error | null;
  handleConnect: (next: string) => void;
  handleDisconnect: () => void;
  retryConnect: () => void;
} {
  const [url, setUrl] = useState(defaultUrl);
  const [client, setClient] = useState<Client | null>(null);
  const [error, setError] = useState<Error | null>(null);

  const currentClientRef = useRef<Client | null>(null);
  const mountedRef = useRef(true);
  const connectGenRef = useRef(0);
  // Mirror so retryConnect stays identity-stable without depending on url state.
  const urlRef = useRef(url);
  urlRef.current = url;

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      if (currentClientRef.current) {
        currentClientRef.current.close();
        currentClientRef.current = null;
      }
    };
  }, []);

  const handleConnect = useCallback((next: string) => {
    setError(null);
    setUrl(next);
    const gen = ++connectGenRef.current;
    void (async () => {
      try {
        // onError can fire from a pending microtask AFTER handleDisconnect bumps the
        // generation; gate so a late delivery doesn't paint over a deliberate disconnect.
        const onErrorGated = (e: Error) => {
          if (!mountedRef.current || gen !== connectGenRef.current) return;
          setError(e);
        };
        const c = await connect({ url: next, onError: onErrorGated });
        if (!mountedRef.current || gen !== connectGenRef.current) {
          c.close();
          return;
        }
        const prev = currentClientRef.current;
        currentClientRef.current = c;
        setClient(c);
        if (prev && prev !== c) prev.close();
      } catch (err) {
        if (!mountedRef.current || gen !== connectGenRef.current) return;
        setError(err instanceof Error ? err : new Error(String(err)));
      }
    })();
  }, []);

  const handleDisconnect = useCallback(() => {
    // Bump first so an in-flight connect self-closes instead of re-installing a torn-down client.
    connectGenRef.current++;
    const c = currentClientRef.current;
    if (c) {
      c.close();
      currentClientRef.current = null;
      setClient(null);
    }
  }, []);

  const retryConnect = useCallback(() => {
    handleConnect(urlRef.current);
  }, [handleConnect]);

  // Auto-connect on first paint when served over http(s). Empty deps: must fire EXACTLY
  // ONCE. `url` here captures the lazy-init seed (only value before handleConnect runs);
  // re-firing on url change would reconnect on every user keystroke in an address bar.
  // Manual reconnect goes through retryConnect.
  useEffect(() => {
    if (typeof window !== "undefined" && window.location.protocol.startsWith("http")) {
      handleConnect(url);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return { client, url, error, handleConnect, handleDisconnect, retryConnect };
}
