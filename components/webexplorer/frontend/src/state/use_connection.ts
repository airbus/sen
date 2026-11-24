// === use_connection.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useRef, useState } from "react";

import type { Client } from "@sen/client";
import { connect } from "@sen/client";

export function normalizeWsUrl(raw: string, pageProtocol?: string): string {
  const trimmed = raw.trim();
  if (!trimmed) return "";
  if (/^wss?:\/\//i.test(trimmed)) {
    return trimmed.replace(/^ws:\/\//i, "ws://").replace(/^wss:\/\//i, "wss://");
  }
  if (/^http:\/\//i.test(trimmed)) {
    return `ws://${trimmed.slice("http://".length)}`;
  }
  if (/^https:\/\//i.test(trimmed)) {
    return `wss://${trimmed.slice("https://".length)}`;
  }
  if (/^[a-z][a-z0-9+.-]*:\/\//i.test(trimmed)) {
    return trimmed;
  }
  const proto =
    (pageProtocol ?? (typeof window !== "undefined" ? window.location.protocol : "http:")) ===
    "https:"
      ? "wss://"
      : "ws://";
  return `${proto}${trimmed}`;
}

// WebSocket lifecycle for the App tree. Three races to handle:
//  - StrictMode double-mount: refs persist; mountedRef re-asserted, real unmount closes.
//  - Connect-then-disconnect: connectGenRef bump makes a late connect resolve into a
//    self-close instead of installing an unowned client.
//  - Rapid URL switches: same generation guard - last connect wins.
// handleConnect/handleDisconnect are identity-stable (state via refs).
export function useConnection(defaultUrl: () => string): {
  client: Client | null;
  url: string;
  failedUrl: string | null;
  error: Error | null;
  connectError: Error | null;
  isConnecting: boolean;
  handleConnect: (next: string) => void;
  cancelConnect: () => void;
  handleDisconnect: () => void;
  retryConnect: () => void;
} {
  const [url, setUrl] = useState(() => normalizeWsUrl(defaultUrl()));
  const [failedUrl, setFailedUrl] = useState<string | null>(null);
  const [client, setClient] = useState<Client | null>(null);
  const [error, setError] = useState<Error | null>(null);
  const [connectError, setConnectError] = useState<Error | null>(null);
  const [isConnecting, setIsConnecting] = useState(false);

  const currentClientRef = useRef<Client | null>(null);
  const connectedUrlRef = useRef<string | null>(null);
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

  const handleConnect = useCallback((rawNext: string) => {
    const next = normalizeWsUrl(rawNext);
    if (!next) return;
    setError(null);
    setConnectError(null);
    setFailedUrl(null);
    setUrl(next);
    setIsConnecting(true);
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
        connectedUrlRef.current = next;
        setClient(c);
        setIsConnecting(false);
        if (prev && prev !== c) prev.close();
      } catch (err) {
        if (!mountedRef.current || gen !== connectGenRef.current) return;
        setConnectError(err instanceof Error ? err : new Error(String(err)));
        setFailedUrl(next);
        setIsConnecting(false);
        if (connectedUrlRef.current !== null) {
          setUrl(connectedUrlRef.current);
        }
      }
    })();
  }, []);

  const cancelConnect = useCallback(() => {
    connectGenRef.current++;
    setIsConnecting(false);
    setConnectError(null);
    setFailedUrl(null);
    if (connectedUrlRef.current !== null) {
      setUrl(connectedUrlRef.current);
    }
  }, []);

  const handleDisconnect = useCallback(() => {
    // Bump first so an in-flight connect self-closes instead of re-installing a torn-down client.
    connectGenRef.current++;
    connectedUrlRef.current = null;
    setIsConnecting(false);
    setConnectError(null);
    setFailedUrl(null);
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

  return {
    client,
    url,
    failedUrl,
    error,
    connectError,
    isConnecting,
    handleConnect,
    cancelConnect,
    handleDisconnect,
    retryConnect,
  };
}
