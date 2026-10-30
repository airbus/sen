// === tool_error.test.ts ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { JsonRpcError, JsonRpcErrorCode, TransportError } from "@sen/client";
import { describe, expect, it } from "vitest";

import { GatewayInputError } from "../../src/errors.js";
import { toolErrorText } from "../../src/tool_error.js";

describe("toolErrorText", () => {
  it("passes a gateway error's message unchanged", () => {
    expect(toolErrorText("invokeMethod", new GatewayInputError("missing 'objectName'"), true)).toBe(
      "missing 'objectName'",
    );
  });

  it("reports a transport error on a kernel that is no longer open as a disconnection", () => {
    const err = new TransportError("connection lost (ws://127.0.0.1:8080)");
    expect(toolErrorText("getProperty", err, false)).toBe("getProperty failed: kernel disconnected mid-call");
  });

  // The call never left the gateway: the client refused to encode it. Calling that a disconnection
  // sends the caller looking for a network fault, when the fix is in its own arguments.
  it("passes on a call the client refused to send while the kernel is open", () => {
    const err = new TransportError('Var encoder: variant "airspace.TaskOrder" has no arm matching tag "HoldOrder"');
    expect(toolErrorText("invokeMethod", err, true)).toBe(
      'invokeMethod failed: Var encoder: variant "airspace.TaskOrder" has no arm matching tag "HoldOrder"',
    );
  });

  it("redacts the credentials of a kernel URL in a transport message", () => {
    const err = new TransportError("connection lost (ws://user:secret@host:8080?token=abc)");
    expect(toolErrorText("invokeMethod", err, true)).toBe(
      "invokeMethod failed: connection lost (ws://host:8080 (redacted))",
    );
  });

  // When a method throws, the kernel's message is generic and the method's own message is in
  // `data`; without it the caller cannot tell why the call was refused.
  it("adds the reason a method gave for failing", () => {
    const err = new JsonRpcError(
      JsonRpcErrorCode.internalError,
      "invoke: handler threw an exception",
      "weapons hold: charlie is civil traffic (neutral), which nobody may fire at",
    );
    expect(toolErrorText("invokeMethod", err, true)).toBe(
      "invokeMethod failed: invoke: handler threw an exception: " +
        "weapons hold: charlie is civil traffic (neutral), which nobody may fire at",
    );
  });

  it("writes structured error data as JSON", () => {
    const err = new JsonRpcError(JsonRpcErrorCode.invalidParams, "invalid params", { field: "x" });
    expect(toolErrorText("setProperty", err, true)).toBe('setProperty failed: invalid params: {"field":"x"}');
  });

  it("leaves out absent error data", () => {
    const err = new JsonRpcError(JsonRpcErrorCode.unknownMember, "unknown member");
    expect(toolErrorText("getProperty", err, true)).toBe("getProperty failed: unknown member");
  });

  it("caps very long error data", () => {
    const err = new JsonRpcError(JsonRpcErrorCode.internalError, "failed", "x".repeat(5000));
    const text = toolErrorText("invokeMethod", err, true);
    expect(text.endsWith("...")).toBe(true);
    expect(text.length).toBeLessThan(2100);
  });

  it("reports any other error by its message", () => {
    expect(toolErrorText("ping", new Error("boom"), true)).toBe("ping failed: boom");
    expect(toolErrorText("ping", "plain", true)).toBe("ping failed: plain");
  });
});
