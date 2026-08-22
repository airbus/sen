#!/usr/bin/env node
// === index.ts ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { TransportError } from "@sen/client";
import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListResourcesRequestSchema,
  ListToolsRequestSchema,
  ReadResourceRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";

import { makeAuditChannel } from "./audit_log.js";
import { PACKAGE_NAME, PACKAGE_VERSION } from "./baked_package.js";
import { GatewayError } from "./errors.js";
import { INSTRUCTIONS } from "./instructions.js";
import { KernelRegistry } from "./kernel_registry.js";
import { RecordingRunner, type RecordingRunnerConfig } from "./recording_runner.js";
import { listResources, readResource } from "./resources.js";
import { makeKernelTools, makeRecordingTools, type GatewayContext, type Tool } from "./tools.js";

async function main(): Promise<void> {
  const log = (msg: string): void => {
    process.stderr.write(`sen-mcp-gateway: ${msg}\n`);
  };
  const registry = new KernelRegistry(log);

  const readonly = isEnvFlagSet("SEN_MCP_GATEWAY_READONLY");
  const noRecording = isEnvFlagSet("SEN_MCP_GATEWAY_NO_RECORDING");
  const auditLogPath = envOrUndefined("SEN_MCP_GATEWAY_AUDIT_LOG");
  const auditChannel = makeAuditChannel(auditLogPath, log);
  const audit = auditChannel.sink;

  const runnerConfig: RecordingRunnerConfig = {};
  let recordingTimeoutError: string | undefined;
  const rawTimeout = envOrUndefined("SEN_RECORDING_TIMEOUT_MS");
  const MAX_RECORDING_TIMEOUT_MS = 600_000;
  if (rawTimeout !== undefined) {
    const parsed = Number(rawTimeout);
    if (!Number.isFinite(parsed) || parsed <= 0 || parsed > MAX_RECORDING_TIMEOUT_MS) {
      recordingTimeoutError =
        `SEN_RECORDING_TIMEOUT_MS must be a finite positive number <= ${MAX_RECORDING_TIMEOUT_MS}, got: ${rawTimeout}`;
      log(`WARNING: ${recordingTimeoutError}; runRecordingScript will fail until this is corrected`);
    } else {
      runnerConfig.timeoutMs = parsed;
    }
  }

  const shutdownController = new AbortController();
  const ctx: GatewayContext = { audit, readonly, shutdownSignal: shutdownController.signal };
  if (recordingTimeoutError !== undefined) ctx.recordingTimeoutError = recordingTimeoutError;

  // Read-only mode has to disable recording too: runRecordingScript runs caller-supplied
  // Python, so a gateway that keeps it while refusing setProperty is read-only in name only.
  const recordingDisabledReason = noRecording
    ? "SEN_MCP_GATEWAY_NO_RECORDING is set"
    : readonly
      ? "SEN_MCP_GATEWAY_READONLY is set and runRecordingScript executes arbitrary Python"
      : undefined;
  if (recordingDisabledReason !== undefined) {
    log(`recording tools not registered: ${recordingDisabledReason}`);
  }
  const recordingRunner = recordingDisabledReason === undefined ? new RecordingRunner(runnerConfig) : null;
  const tools: Tool[] = [...makeKernelTools(ctx)];
  if (recordingRunner !== null) {
    tools.push(...makeRecordingTools(recordingRunner, ctx));
  }
  const toolByName = new Map(tools.map((t) => [t.name, t]));

  const server = new Server(
    { name: PACKAGE_NAME, version: PACKAGE_VERSION },
    {
      capabilities: { tools: {}, resources: {} },
      instructions: INSTRUCTIONS,
    },
  );

  server.setRequestHandler(ListToolsRequestSchema, async () => ({
    tools: tools.map((t) => ({
      name: t.name,
      description: t.description,
      inputSchema: t.inputSchema,
    })),
  }));

  server.setRequestHandler(ListResourcesRequestSchema, async () => ({
    resources: listResources(),
  }));

  server.setRequestHandler(ReadResourceRequestSchema, async (request) => {
    return readResource(request.params.uri);
  });

  server.setRequestHandler(CallToolRequestSchema, async (request) => {
    const { name, arguments: args } = request.params;
    const tool = toolByName.get(name);
    if (tool === undefined) {
      return { isError: true, content: [{ type: "text", text: `unknown tool: ${name}` }] };
    }
    try {
      return await tool.handler(registry, (args as Record<string, unknown> | undefined) ?? {});
    } catch (err) {
      if (err instanceof GatewayError) {
        return { isError: true, content: [{ type: "text", text: err.message }] };
      }
      if (err instanceof TransportError) {
        return { isError: true, content: [{ type: "text", text: `${name} failed: kernel disconnected mid-call` }] };
      }
      const text = `${name} failed: ${err instanceof Error ? err.message : String(err)}`;
      return { isError: true, content: [{ type: "text", text }] };
    }
  });

  // Wire onclose before connect so an immediate transport close isn't missed. Abort the
  // shared signal first so recording-runner children and kernel RPCs cancel together; cap
  // the wall-clock so a stuck child never blocks gateway exit.
  const SHUTDOWN_BUDGET_MS = 2_000;
  let shuttingDown = false;
  const shutdown = (): void => {
    if (shuttingDown) return;
    shuttingDown = true;
    shutdownController.abort();
    const drainTasks: Promise<unknown>[] = [registry.shutdown(), auditChannel.close()];
    if (recordingRunner !== null) drainTasks.push(recordingRunner.drain());
    const cap = new Promise<void>((resolve) => setTimeout(resolve, SHUTDOWN_BUDGET_MS).unref());
    void Promise.race([Promise.allSettled(drainTasks), cap]).finally(() => process.exit(0));
  };
  server.onclose = shutdown;
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);

  const transport = new StdioServerTransport();
  await server.connect(transport);
}

function envOrUndefined(name: string): string | undefined {
  const v = process.env[name];
  if (v === undefined || v === "") return undefined;
  return v;
}

function isEnvFlagSet(name: string): boolean {
  const v = process.env[name];
  if (v === undefined) return false;
  const lc = v.toLowerCase();
  return lc === "1" || lc === "true" || lc === "yes" || lc === "on";
}

main().catch((err: unknown) => {
  process.stderr.write(`sen-mcp-gateway fatal: ${err instanceof Error ? err.stack ?? err.message : String(err)}\n`);
  process.exit(1);
});
