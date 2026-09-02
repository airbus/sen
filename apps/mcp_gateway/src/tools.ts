// === tools.ts ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client, CustomTypeSpec, InterestHandle, ObjectHandle } from "@sen/client";
import { createHash } from "node:crypto";
import { lstat, readdir, realpath, stat } from "node:fs/promises";
import { join, sep } from "node:path";
import type { AuditEntry, AuditSink } from "./audit_log.js";
import { BAKED_DOCS_BY_URI } from "./baked_docs.js";
import { GatewayInputError, GatewayStateError } from "./errors.js";
import { RecordingRunnerInputTooLargeError, type RecordingRunner } from "./recording_runner.js";
import { EVENT_BUFFER_BYTE_CAP, EVENT_BUFFER_CAP, INTEREST_CAP_PER_KERNEL, type Kernel } from "./kernel.js";
import type { KernelRegistry } from "./kernel_registry.js";
import { redactUrlForAudit } from "./util/redact_url.js";

export interface GatewayContext {
  audit: AuditSink;
  readonly: boolean;
  // Aborts at gateway shutdown so tool handlers (recording runner, future async work) cancel
  // cleanly instead of orphaning children.
  shutdownSignal: AbortSignal;
  recordingTimeoutError?: string;
}

type ToolResult = {
  isError?: boolean;
  content: Array<{ type: "text"; text: string }>;
};

const KERNEL_SCHEMA = {
  type: "string",
  description:
    "Name of the kernel to target (from connectToKernel). Optional when exactly one kernel is connected.",
} as const;

const INTEREST_NAME_SCHEMA = {
  type: "string",
  description: "Name of an interest opened via declareInterest. Scoped to the targeted kernel.",
} as const;

const OBJECT_NAME_SCHEMA = {
  type: "string",
  description: "Name of an object currently in the interest's match-set (see listObjects).",
} as const;

const PROPERTY_NAME_SCHEMA = {
  type: "string",
  description: "Property name on the object's class or any parent (see getType, PropertySpec).",
} as const;

const EVENT_NAME_SCHEMA = {
  type: "string",
  description: "Event name on the object's class or any parent (see getType, EventSpec).",
} as const;

export interface Tool {
  name: string;
  description: string;
  inputSchema: object;
  handler: (registry: KernelRegistry, args: Record<string, unknown>) => Promise<ToolResult>;
}

// i64 / u64 arrive from @sen/client as bigint and JSON.stringify rejects them. Emit as
// JSON strings so values above Number.MAX_SAFE_INTEGER survive the LLM hop; the
// setProperty / invokeMethod encoders accept decimal-string or number on the way back.
export function jsonReplacer(_key: string, value: unknown): unknown {
  return typeof value === "bigint" ? value.toString() : value;
}

function textOk(payload: unknown): ToolResult {
  const text = typeof payload === "string" ? payload : JSON.stringify(payload, jsonReplacer, 2);
  return { content: [{ type: "text", text }] };
}

// Logged whether or not it succeeded: a mutation that reaches the kernel and then loses the
// response has still been applied. Failures carry the error class, never its message.
async function audited<T>(ctx: GatewayContext, entry: AuditEntry, effect: () => T | Promise<T>): Promise<T> {
  try {
    const result = await effect();
    ctx.audit({ ...entry, outcome: "ok" });
    return result;
  } catch (err) {
    ctx.audit({ ...entry, outcome: "failed", error: err instanceof Error ? err.name : "unknown" });
    throw err;
  }
}

// Tool handlers throw typed GatewayError; the dispatcher in index.ts formats them. Handlers
// must never return an isError ToolResult directly.

function requireString(args: Record<string, unknown>, key: string): string {
  const v = args[key];
  if (typeof v !== "string") throw new GatewayInputError(`missing required string argument: ${key}`);
  return v;
}

function asString(args: Record<string, unknown>, key: string): string | undefined {
  const v = args[key];
  return typeof v === "string" ? v : undefined;
}

function asBool(args: Record<string, unknown>, key: string): boolean | undefined {
  const v = args[key];
  return typeof v === "boolean" ? v : undefined;
}

// SDK JSON-Schema validation should reject wrong shapes; the typed throw guards hand-crafted
// callers that bypass it.
function asStringArray(args: Record<string, unknown>, key: string): string[] | null {
  const v = args[key];
  if (v === undefined) return null;
  if (!Array.isArray(v)) throw new GatewayInputError(`expected '${key}' to be an array of strings`);
  for (const entry of v) {
    if (typeof entry !== "string") {
      throw new GatewayInputError(`'${key}' must contain only strings; got ${entry === null ? "null" : typeof entry}`);
    }
  }
  return v as string[];
}

function resolveLink(registry: KernelRegistry, args: Record<string, unknown>): Kernel {
  const link = registry.resolve(asString(args, "kernel"));
  link.assertOpen();
  return link;
}

function requireInterest(link: Kernel, interestName: string): InterestHandle {
  const handle = link.interests.get(interestName);
  if (handle === undefined) throw new GatewayStateError(`no interest open with name: ${interestName}`);
  return handle;
}

function requireObject(handle: InterestHandle, interestName: string, objectName: string): ObjectHandle {
  const obj = handle.objectByName(objectName);
  if (obj === undefined) {
    throw new GatewayStateError(`object not in interest match-set: ${objectName} (interest=${interestName})`);
  }
  return obj;
}

// invokeMethod is withdrawn in read-only mode, not filtered by method: `[const]` promises a
// method does not modify its own object, not that it has no effect. The shell's
// `fn shutdown() [const]` stops the kernel.
export function makeKernelTools(ctx: GatewayContext): Tool[] {
  const tools: Tool[] = [
  {
    name: "ping",
    description: "Health check. Returns 'pong'. Use to verify the Sen MCP gateway is reachable.",
    inputSchema: { type: "object", properties: {}, additionalProperties: false },
    handler: async () => textOk("pong"),
  },
  {
    name: "connectToKernel",
    description:
      "Open a WebSocket to a Sen kernel and register it under `name`. The gateway can hold " +
      "zero or many kernel connections simultaneously; subsequent tool calls reference the kernel " +
      "by `name`. Returns `{connected: name}` once the WebSocket is open; failure to connect throws.",
    inputSchema: {
      type: "object",
      properties: {
        name: { type: "string", description: "Caller-chosen name. Non-empty, no whitespace, unique." },
        url: { type: "string", description: "WebSocket URL, e.g. ws://127.0.0.1:8080." },
        openTimeoutMs: {
          type: "number",
          minimum: 1,
          maximum: 60_000,
          description: "Connection timeout in milliseconds. Defaults to 5000; capped at 60000.",
        },
      },
      required: ["name", "url"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const name = requireString(args, "name");
      const url = requireString(args, "url");
      const rawTimeout = args["openTimeoutMs"];
      const openTimeoutMs =
        typeof rawTimeout === "number" && Number.isFinite(rawTimeout) && rawTimeout > 0
          ? Math.min(rawTimeout, 60_000)
          : 5_000;
      await audited(ctx, { tool: "connectToKernel", name, url: redactUrlForAudit(url) }, () =>
        registry.connect(name, url, openTimeoutMs),
      );
      return textOk({ connected: name });
    },
  },
  {
    name: "disconnectFromKernel",
    description:
      "Close the connection to a kernel and tear down all interests, event subscriptions, and " +
      "buffers attached to it. Subsequent tool calls referencing that kernel will fail. " +
      "Returns `{disconnected: name}`.",
    inputSchema: {
      type: "object",
      properties: { name: { type: "string" } },
      required: ["name"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const name = requireString(args, "name");
      await audited(ctx, { tool: "disconnectFromKernel", name }, () => registry.disconnect(name));
      return textOk({ disconnected: name });
    },
  },
  {
    name: "listKernels",
    description: "List every currently-connected kernel as `{name, url, state}`. State is the wire-level connection state.",
    inputSchema: { type: "object", properties: {}, additionalProperties: false },
    handler: async (registry) =>
      textOk(registry.list().map((l) => ({ name: l.name, url: l.url, state: l.state }))),
  },
  {
    name: "listSessions",
    description:
      "List sessions reachable on the targeted kernel and the buses each one exposes. " +
      "Queries (declareInterest) always reference buses as `<session>.<bus>`; never " +
      "the bare bus name. Combine the session `name` field with each entry in `buses` " +
      "to build the qualified form.",
    inputSchema: {
      type: "object",
      properties: { kernel: KERNEL_SCHEMA },
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const link = resolveLink(registry, args);
      const client = await link.client();
      const sessions = await client.listTopology({ signal: link.signal });
      return textOk(sessions);
    },
  },
  {
    name: "getTypes",
    description: "List every qualified type name registered on the targeted kernel.",
    inputSchema: {
      type: "object",
      properties: { kernel: KERNEL_SCHEMA },
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const link = resolveLink(registry, args);
      const client = await link.client();
      const names = await client.fetchTypeNames({ signal: link.signal });
      return textOk(names);
    },
  },
  {
    name: "getType",
    description:
      "Fetch a type spec by qualified name from the targeted kernel. Pass withSchema:true to " +
      "also include the JSON-Schema fragment.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        qualifiedName: { type: "string", description: "Qualified type name (e.g. 'demo.Aircraft')." },
        withSchema: { type: "boolean", description: "If true, also return the JSON-Schema fragment." },
      },
      required: ["qualifiedName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const qualifiedName = requireString(args, "qualifiedName");
      const withSchema = asBool(args, "withSchema") === true;
      const link = resolveLink(registry, args);
      const client = await link.client();
      const opts: { withSchema?: boolean; signal: AbortSignal } = { signal: link.signal };
      if (withSchema) opts.withSchema = true;
      const result = await client.fetchType(qualifiedName, opts);
      return textOk(result);
    },
  },
  {
    name: "declareInterest",
    description:
      "Open an interest (live query) on the targeted kernel. Returns the initial match-set " +
      `as {kernel, name, matched}. Interest names are scoped per kernel; the gateway caps ` +
      `each kernel at ${INTEREST_CAP_PER_KERNEL} open interests.`,
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        name: { type: "string", description: "Caller-chosen name; used to reference the interest in later calls." },
        query: { type: "string", description: "Sen Query Language (e.g. 'SELECT demo.Aircraft FROM tutorial.pets')." },
        withSchemas: {
          type: "boolean",
          description: "If true, JSON-Schema fragments for newly-seen types are kept alongside the specs.",
        },
      },
      required: ["name", "query"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const name = requireString(args, "name");
      const query = requireString(args, "query");
      const link = resolveLink(registry, args);
      // Atomic dup-name + cap reservation against concurrent declareInterest: the guard
      // here catches the race instead of letting the loser's handle leak.
      link.reserveInterestSlot(name);
      try {
        const withSchemas = asBool(args, "withSchemas") === true;
        const client = await link.client();
        const declareArgs: { name: string; query: string; withSchemas?: boolean; signal: AbortSignal } = {
          name,
          query,
          signal: link.signal,
        };
        if (withSchemas) declareArgs.withSchemas = true;
        // `query` is logged in full on purpose, unlike the kernel URL above. It is the interest
        // expression -- what the model asked to watch -- which is the question this log exists to
        // answer. Reducing it would leave entries that record that something was watched without
        // recording what, and it carries no credential: it names objects and properties.
        const handle = await audited(ctx, { tool: "declareInterest", kernel: link.name, name, query }, () =>
          client.declareInterest(declareArgs),
        );
        link.commitInterest(name, handle);
        const matched = handle.objects().map((o) => ({ name: o.name, className: o.className }));
        return textOk({ kernel: link.name, name, matched });
      } catch (err) {
        link.releaseInterestSlot(name);
        throw err;
      }
    },
  },
  {
    name: "releaseInterest",
    description:
      "Close an open interest by name on the targeted kernel. Frees the name for reuse. " +
      "Returns `{released: name, drainedEvents}` where `drainedEvents` carries every event " +
      "still buffered for this interest at release time (a caller that subscribes, triggers, " +
      "and releases without an explicit pollEvents does not silently lose deliveries).",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        name: INTEREST_NAME_SCHEMA,
      },
      required: ["name"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const name = requireString(args, "name");
      const link = resolveLink(registry, args);
      const handle = requireInterest(link, name);
      const drainedEvents = link.drainEvents(name);
      // Tear down event subs first so handlers stop firing during release.
      link.tearDownInterestEvents(name);
      link.interests.delete(name);
      await audited(ctx, { tool: "releaseInterest", kernel: link.name, name }, () => handle.release());
      return textOk({ released: name, drainedEvents });
    },
  },
  {
    name: "listInterests",
    description:
      "List every interest currently open. Without `kernel`, returns interests across every " +
      "connected kernel as `[{kernel, name}]`. With `kernel`, restricted to that one kernel.",
    inputSchema: {
      type: "object",
      properties: { kernel: KERNEL_SCHEMA },
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const kernel = asString(args, "kernel");
      const links = kernel !== undefined ? [registry.resolve(kernel)] : registry.list();
      const entries: Array<{ kernel: string; name: string }> = [];
      for (const link of links) {
        for (const name of link.interests.keys()) entries.push({ kernel: link.name, name });
      }
      return textOk(entries);
    },
  },
  {
    name: "listObjects",
    description:
      "List objects currently matching an open interest. Returns " +
      "`{interest, objects: [{name, className}, ...]}`. The match-set is live: subsequent " +
      "calls may return different entries.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        name: INTEREST_NAME_SCHEMA,
      },
      required: ["name"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const name = requireString(args, "name");
      const link = resolveLink(registry, args);
      const handle = link.interests.get(name);
      if (handle === undefined) throw new GatewayStateError(`no interest open with name: ${name}`);
      const objects = handle.objects().map((o) => ({ name: o.name, className: o.className }));
      return textOk({ interest: name, objects });
    },
  },
  {
    name: "getProperty",
    description:
      "Read one property of an object reachable through an open interest. Returns the parsed Var " +
      "(primitives as JS numbers/booleans/strings, structs as nested objects, etc.). 64-bit " +
      "integer fields (`i64`, `u64`) appear as JSON strings (e.g. `\"12345\"`) to preserve " +
      "precision above 2^53; treat them as integers in the caller.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
        objectName: OBJECT_NAME_SCHEMA,
        propertyName: PROPERTY_NAME_SCHEMA,
      },
      required: ["interestName", "objectName", "propertyName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const objectName = requireString(args, "objectName");
      const propertyName = requireString(args, "propertyName");
      const link = resolveLink(registry, args);
      const handle = requireInterest(link, interestName);
      const obj = requireObject(handle, interestName, objectName);
      const value = await obj.get(propertyName, { fresh: true, signal: link.signal });
      return textOk({ object: objectName, property: propertyName, value });
    },
  },
  {
    name: "getObjectsState",
    description:
      "Read every property of one or many objects in a single round-trip. `objectNames` " +
      "filters the match-set; omit for every matched object. `propertyNames` filters which " +
      "properties to read; omit for every property (server walks inheritance). Each returned " +
      "object carries `properties` (successful reads) and `errors` (per-property failures, " +
      "e.g. unknown name or a variant with no current value); object names not in the match-set " +
      "appear under `notFound` so the caller knows which requests fell through.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
        objectNames: {
          type: "array",
          items: OBJECT_NAME_SCHEMA,
          minItems: 1,
          description: "Subset of object names to fetch. Omit to fetch every matched object.",
        },
        propertyNames: {
          type: "array",
          items: PROPERTY_NAME_SCHEMA,
          minItems: 1,
          description: "Subset of property names to fetch. Omit to fetch every property (walking inheritance).",
        },
      },
      required: ["interestName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const link = resolveLink(registry, args);
      const handle = requireInterest(link, interestName);

      const requestedNames = asStringArray(args, "objectNames");
      const explicitProperties = asStringArray(args, "propertyNames");

      const opts: Parameters<typeof handle.getObjectsBatchState>[0] = { signal: link.signal };
      if (requestedNames !== null) opts.objectNames = requestedNames;
      if (explicitProperties !== null) opts.propertyNames = explicitProperties;
      const states = await handle.getObjectsBatchState(opts);

      const objects = states.map((s) => ({ name: s.name, className: s.className, properties: s.properties, errors: s.errors }));
      const payload: { interest: string; objects: typeof objects; notFound?: string[] } = { interest: interestName, objects };
      if (requestedNames !== null) {
        const returned = new Set(states.map((s) => s.name));
        const notFound = requestedNames.filter((n) => !returned.has(n));
        if (notFound.length > 0) payload.notFound = notFound;
      }
      return textOk(payload);
    },
  },
  {
    name: "setProperty",
    description:
      "Write a property's value. Returns `{ok:true}` once the server has accepted the write. " +
      "Value shape follows the property's JSON-Schema.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
        objectName: OBJECT_NAME_SCHEMA,
        propertyName: PROPERTY_NAME_SCHEMA,
        value: {
          description: "New value, shaped per the property's JSON-Schema (primitives, struct objects, etc.).",
        },
      },
      required: ["interestName", "objectName", "propertyName", "value"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const objectName = requireString(args, "objectName");
      const propertyName = requireString(args, "propertyName");
      if (!("value" in args)) throw new GatewayInputError("missing required argument: value");
      // Refuse before resolving the kernel: a denial has to reach the audit log even when the
      // call also names a kernel, interest or object that does not exist, which is exactly the
      // shape a probe takes. Resolving first also answers with the connected kernel names.
      if (ctx.readonly) {
        ctx.audit({
          tool: "setProperty",
          kernel: typeof args["kernel"] === "string" ? args["kernel"] : undefined,
          interestName,
          objectName,
          propertyName,
          outcome: "denied",
        });
        throw new GatewayStateError("setProperty is disabled: gateway is in read-only mode");
      }
      const link = resolveLink(registry, args);
      const entry: AuditEntry = {
        tool: "setProperty",
        kernel: link.name,
        interestName,
        objectName,
        propertyName,
      };
      const handle = requireInterest(link, interestName);
      const obj = requireObject(handle, interestName, objectName);
      await audited(ctx, entry, () => obj.set(propertyName, args["value"], { signal: link.signal }));
      return textOk({ ok: true });
    },
  },
  {
    name: "invokeMethod",
    description:
      "Call a method on an object reachable through an open interest. `args` is keyed by " +
      "parameter name (e.g. `{x: 1, y: 2}`). Returns `{object, method, result}` where " +
      "`result` is the parsed return value (null for void methods).",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
        objectName: OBJECT_NAME_SCHEMA,
        methodName: {
          type: "string",
          description: "Method name on the object's class or any parent (see getType, MethodSpec).",
        },
        args: {
          type: "object",
          description: "Map of parameter name -> value. Omit for zero-arg methods (defaults to {}).",
          additionalProperties: true,
        },
      },
      required: ["interestName", "objectName", "methodName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const objectName = requireString(args, "objectName");
      const methodName = requireString(args, "methodName");
      const link = resolveLink(registry, args);
      const handle = requireInterest(link, interestName);
      const obj = requireObject(handle, interestName, objectName);
      const entry: AuditEntry = {
        tool: "invokeMethod",
        kernel: link.name,
        interestName,
        objectName,
        methodName,
      };
      const methodArgs = (args["args"] as Record<string, unknown> | undefined) ?? {};
      const result = await audited(ctx, entry, () => obj.invoke(methodName, methodArgs, { signal: link.signal }));
      return textOk({ object: objectName, method: methodName, result });
    },
  },
  {
    name: "subscribeEvent",
    description:
      "Start buffering one event on one object reachable through an open interest. Deliveries " +
      `accumulate in a per-interest FIFO (capped at ${EVENT_BUFFER_CAP} entries or ` +
      `${EVENT_BUFFER_BYTE_CAP / (1024 * 1024)} MiB, whichever comes first, drop-oldest) until you call ` +
      "pollEvents. All events subscribed under the same interest share that buffer, so a chatty " +
      "event can starve a rare one; subscribe rare events under their own interest if drop-oldest " +
      "is a problem. Duplicate subscribes are no-ops. releaseInterest drains and returns the buffer " +
      "alongside the released confirmation. Returns " +
      "`{subscribed: {interestName, objectName, eventName}, newSubscription}` where " +
      "`newSubscription` is true on first subscribe, false on duplicate.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
        objectName: OBJECT_NAME_SCHEMA,
        eventName: EVENT_NAME_SCHEMA,
      },
      required: ["interestName", "objectName", "eventName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const objectName = requireString(args, "objectName");
      const eventName = requireString(args, "eventName");
      const link = resolveLink(registry, args);
      const handle = requireInterest(link, interestName);
      requireObject(handle, interestName, objectName);
      const installed = await audited(
        ctx,
        { tool: "subscribeEvent", kernel: link.name, interestName, objectName, eventName },
        () => link.subscribeToEvent(interestName, objectName, eventName),
      );
      return textOk({ subscribed: { interestName, objectName, eventName }, newSubscription: installed });
    },
  },
  {
    name: "unsubscribeEvent",
    description:
      "Cancel one event subscription on an interest. Already-buffered events for the same " +
      "interest stay in the buffer until polled or until releaseInterest fires. Returns " +
      "{removed:false} if no subscription was open for that (object, event) pair.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
        objectName: OBJECT_NAME_SCHEMA,
        eventName: EVENT_NAME_SCHEMA,
      },
      required: ["interestName", "objectName", "eventName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const objectName = requireString(args, "objectName");
      const eventName = requireString(args, "eventName");
      const link = resolveLink(registry, args);
      const removed = await audited(
        ctx,
        { tool: "unsubscribeEvent", kernel: link.name, interestName, objectName, eventName },
        () => link.unsubscribeFromEvent(interestName, objectName, eventName),
      );
      return textOk({ removed });
    },
  },
  {
    name: "unsubscribeAllEvents",
    description:
      "Cancel every event subscription open under an interest in one call. Returns " +
      "`{cancelled: count}`. Already-buffered events stay in the buffer until polled or " +
      "until releaseInterest fires.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
      },
      required: ["interestName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const link = resolveLink(registry, args);
      requireInterest(link, interestName);
      const cancelled = await audited(
        ctx,
        { tool: "unsubscribeAllEvents", kernel: link.name, interestName },
        () => link.unsubscribeAllEventsForInterest(interestName),
      );
      return textOk({ cancelled });
    },
  },
  {
    name: "pollEvents",
    description:
      "Drain every event buffered for an interest since the last poll. Each entry is " +
      "{objectName, eventName, args, timestamp}. Returns [] when no events are buffered. " +
      "Buffer is flushed on success; a failed call leaves the buffer intact.",
    inputSchema: {
      type: "object",
      properties: {
        kernel: KERNEL_SCHEMA,
        interestName: INTEREST_NAME_SCHEMA,
      },
      required: ["interestName"],
      additionalProperties: false,
    },
    handler: async (registry, args) => {
      const interestName = requireString(args, "interestName");
      const link = resolveLink(registry, args);
      requireInterest(link, interestName);
      const events = await audited(ctx, { tool: "pollEvents", kernel: link.name, interestName }, () =>
        link.drainEvents(interestName),
      );
      return textOk({ interest: interestName, events });
    },
  },
  ];
  return ctx.readonly ? tools.filter((tool) => tool.name !== "invokeMethod") : tools;
}

export function makeRecordingTools(runner: RecordingRunner, ctx: GatewayContext): Tool[] {
  // runRecordingScript executes caller-supplied Python, which no per-call check can constrain
  // the way setProperty and invokeMethod are constrained. Read-only mode therefore drops the
  // whole group rather than advertising a surface it cannot police.
  if (ctx.readonly) return [];
  return [
    {
      name: "listRecordings",
      description:
        "Enumerate recordings under `root`. Each entry is {path, sizeBytes, mtime}. " +
        "Recordings must be direct children of the root (the scan is not recursive). " +
        "Pass a glob (e.g. 'school_*') to filter by basename. The user must supply " +
        "`root`; ask for it if they haven't. Symlinks whose targets resolve outside " +
        "`root` are dropped, but a symlink whose target is *inside* `root` is followed " +
        "and returned verbatim; the recordings directory is not a security boundary, " +
        "and a deployment that exposes untrusted recordings should mount the volume " +
        "read-only.",
      inputSchema: {
        type: "object",
        properties: {
          root: { type: "string", description: "Absolute filesystem path of the recordings folder." },
          glob: {
            type: "string",
            maxLength: MAX_GLOB_LENGTH,
            description: "Optional basename glob (fnmatch-style).",
          },
        },
        required: ["root"],
        additionalProperties: false,
      },
      handler: async (_registry, args) => {
        const root = requireString(args, "root");
        const glob = asString(args, "glob");
        const entries = await listRecordings(root, glob);
        return textOk(entries);
      },
    },
    {
      name: "runRecordingScript",
      description:
        "Run a Python script against sen_db_python. The script is self-contained: import " +
        "sen_db_python as sen, open recordings with sen.Input(<literal path>), aggregate, " +
        "print. stdout cap 64 KiB / stderr cap 16 KiB / wall-clock 60s. Returns " +
        "{stdout, stderr, exitCode, durationMs, stdoutTruncated, stderrTruncated}. Fetch " +
        "the bindings reference via getRecordingDocs (or read the " +
        "sen://docs/users-guide/db-python-bindings resource) before authoring.",
      inputSchema: {
        type: "object",
        properties: { code: { type: "string", description: "Python source to exec." } },
        required: ["code"],
        additionalProperties: false,
      },
      handler: async (_registry, args) => {
        if (ctx.recordingTimeoutError !== undefined) {
          throw new GatewayStateError(`runRecordingScript unavailable: ${ctx.recordingTimeoutError}`);
        }
        const code = requireString(args, "code");
        // The script itself stays out of the log (it can carry values); the digest still ties
        // an entry to a specific body, which byte counts alone never could.
        const codeSha256 = createHash("sha256").update(code, "utf8").digest("hex");
        let result;
        try {
          result = await runner.run(code, { signal: ctx.shutdownSignal });
        } catch (err) {
          ctx.audit({
            tool: "runRecordingScript",
            codeSha256,
            outcome: "failed",
            error: err instanceof Error ? err.name : "unknown",
          });
          if (err instanceof RecordingRunnerInputTooLargeError) {
            throw new GatewayInputError(err.message);
          }
          throw err;
        }
        ctx.audit({
          tool: "runRecordingScript",
          codeSha256,
          outcome: "ok",
          exitCode: result.exitCode,
          signal: result.signal,
          spawnFailed: result.spawnFailed,
          timedOut: result.timedOut,
          aborted: result.aborted,
          durationMs: result.durationMs,
          stdoutBytes: Buffer.byteLength(result.stdout, "utf8"),
          stderrBytes: Buffer.byteLength(result.stderr, "utf8"),
          stdoutTruncated: result.stdoutTruncated,
          stderrTruncated: result.stderrTruncated,
        });
        return textOk(result);
      },
    },
    {
      name: "getRecordingDocs",
      description:
        "Return the sen_db_python bindings reference (markdown). Call this before authoring a " +
        "runRecordingScript when you need the cursor / payload / type-registry / annotation API.",
      inputSchema: { type: "object", properties: {}, additionalProperties: false },
      handler: async () => {
        const doc = BAKED_DOCS_BY_URI.get("sen://docs/users-guide/db-python-bindings");
        if (doc === undefined) {
          throw new Error("baked corpus missing sen://docs/users-guide/db-python-bindings; bake_docs.mjs regression");
        }
        return textOk(doc.content);
      },
    },
  ];
}

// Hidden entries (.) skipped. Glob is fnmatch on the basename. Symlinks are followed only
// when their realpath resolves inside `root`; out-of-bounds links are dropped silently so
// target metadata doesn't leak.
async function listRecordings(
  root: string,
  glob: string | undefined,
): Promise<Array<{ path: string; sizeBytes: number; mtime: string }>> {
  const matcher = glob !== undefined ? compileGlob(glob) : undefined;
  const rootReal = await realpath(root);
  const rootBoundary = rootReal.endsWith(sep) ? rootReal : rootReal + sep;
  const entries: Array<{ path: string; sizeBytes: number; mtime: string }> = [];
  for (const name of await readdir(root)) {
    if (name.startsWith(".")) continue;
    if (matcher !== undefined && !matcher.test(name)) continue;
    const abs = join(root, name);
    try {
      const ls = await lstat(abs);
      if (ls.isSymbolicLink()) {
        const real = await realpath(abs);
        if (real !== rootReal && !real.startsWith(rootBoundary)) continue;
      }
      const s = await stat(abs);
      entries.push({ path: abs, sizeBytes: s.size, mtime: s.mtime.toISOString() });
    } catch {
      // skip entries that vanished, dangling symlinks, etc.
    }
  }
  entries.sort((a, b) => a.path.localeCompare(b.path));
  return entries;
}

export interface GlobMatcher {
  test(name: string): boolean;
}

// Longest basename any filesystem accepts is 255 bytes, so a pattern past this is never a
// filter, only a load generator.
export const MAX_GLOB_LENGTH = 256;

// fnmatch matcher for * and ?, anchored to the whole basename. Not a RegExp. One `.*` per star
// backtracks exponentially, and an 18-character pattern was enough to stall this single-threaded
// process for nearly two minutes.
export function compileGlob(pattern: string): GlobMatcher {
  if (pattern.length > MAX_GLOB_LENGTH) {
    throw new GatewayInputError(`glob too long (${pattern.length} chars; max ${MAX_GLOB_LENGTH})`);
  }
  return { test: (name: string) => globMatch(pattern, name) };
}

// Two-pointer walk: on a mismatch the most recent star swallows one more character rather
// than the matcher exploring alternatives, so the cost is bounded by pattern x name.
function globMatch(pattern: string, name: string): boolean {
  let p = 0;
  let n = 0;
  let starAt = -1;
  let starTaken = 0;
  while (n < name.length) {
    const pc = pattern[p];
    if (pc === "*") {
      starAt = p++;
      starTaken = n;
    } else if (pc === "?" || pc === name[n]) {
      p++;
      n++;
    } else if (starAt >= 0) {
      p = starAt + 1;
      n = ++starTaken;
    } else {
      return false;
    }
  }
  while (pattern[p] === "*") p++;
  return p === pattern.length;
}
