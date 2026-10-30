// === tool_error.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { JsonRpcError, TransportError } from "@sen/client";

import { GatewayError } from "./errors.js";
import { redactUrlForAudit } from "./util/redact_url.js";

// A method's own error message is rarely longer; this only bounds a pathological one.
const MAX_DETAIL_CHARS = 2000;

/**
 * The text a failed tool call returns to the host.
 *
 * `@sen/client` raises TransportError both when the connection goes and when it refuses to send a
 * call: an unknown method, a missing argument, a value that does not fit its type (a variant tag
 * that names no arm, for one). Only the first is a disconnection, so `kernelOpen` says whether the
 * kernel's connection was still open; the others carry a message the caller can act on. When a
 * method throws, the kernel answers with a JsonRpcError whose `data` holds the method's own
 * message, the only place the reason for the failure appears.
 */
export function toolErrorText(tool: string, err: unknown, kernelOpen: boolean): string {
  if (err instanceof GatewayError) {
    return err.message;
  }
  if (err instanceof TransportError) {
    return kernelOpen ? `${tool} failed: ${redactUrls(err.message)}` : `${tool} failed: kernel disconnected mid-call`;
  }
  if (err instanceof JsonRpcError) {
    const detail = describeData(err.data);
    return detail === "" ? `${tool} failed: ${err.message}` : `${tool} failed: ${err.message}: ${detail}`;
  }
  return `${tool} failed: ${err instanceof Error ? err.message : String(err)}`;
}

function describeData(data: unknown): string {
  if (data === undefined || data === null) {
    return "";
  }
  const text = typeof data === "string" ? data : JSON.stringify(data);
  return text.length > MAX_DETAIL_CHARS ? `${text.slice(0, MAX_DETAIL_CHARS)}...` : text;
}

// The client names the kernel URL in its connection messages; show it as the audit log does.
function redactUrls(message: string): string {
  return message.replace(/wss?:\/\/[^\s)]+/g, (url) => redactUrlForAudit(url));
}
