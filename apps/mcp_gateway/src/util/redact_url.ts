// === redact_url.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Kernel URLs reach the audit log, which is an append-only file that outlives the session.
//
// No URL carries a secret today, because there is no way to authenticate to a kernel yet. That
// is precisely why this is cheap to add now. When authentication does arrive it will most
// likely arrive in the URL: the jsonrpc server reads its credential from the `authorization`
// request header, and the browser WebSocket API cannot set request headers at all, which leaves
// a query parameter or userinfo as the only places a browser client can put one.
//
// So the audit log is on course to record credentials verbatim, and the fix is to stop carrying
// the parts that could hold one before that happens rather than after.

/// Scheme, host and port only. Userinfo, query and fragment are dropped, and their removal is
/// marked so a reader can tell a bare URL from a trimmed one. An unparseable value is withheld
/// entirely: it cannot be shown to be free of a credential, and a typo'd host with a real
/// password in it is exactly the shape that fails to parse.
export function redactUrlForAudit(raw: string): string {
  let parsed: URL;
  try {
    parsed = new URL(raw);
  } catch {
    return "<unparseable url>";
  }
  const carried =
    parsed.username !== "" || parsed.password !== "" || parsed.search !== "" || parsed.hash !== "";
  const path = parsed.pathname === "/" ? "" : parsed.pathname;
  const base = `${parsed.protocol}//${parsed.host}${path}`;
  return carried ? `${base} (redacted)` : base;
}
