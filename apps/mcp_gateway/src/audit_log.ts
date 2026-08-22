// === audit_log.ts ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { closeSync, createWriteStream, openSync, type WriteStream } from "node:fs";

export type AuditEntry = Record<string, unknown> & { tool: string };

export type AuditSink = (entry: AuditEntry) => void;

export interface AuditChannel {
  sink: AuditSink;
  close(): Promise<void>;
}

// openSync validates the path synchronously; later writes are async and never block the
// dispatcher. On any stream error the sink latches over to stderr exactly once so a broken
// path or a full disk doesn't slowly drown the log channel.
export function makeAuditChannel(
  filePath: string | undefined,
  log: (msg: string) => void,
): AuditChannel {
  const pid = process.pid;
  let stream: WriteStream | undefined;
  let latchedToStderr = filePath === undefined;
  if (filePath !== undefined) {
    try {
      closeSync(openSync(filePath, "a"));
      stream = createWriteStream(filePath, { flags: "a" });
      stream.on("error", (err) => {
        if (latchedToStderr) return;
        latchedToStderr = true;
        log(`audit-log: stream to ${filePath} failed (${err.message}); falling back to stderr`);
      });
    } catch (err) {
      latchedToStderr = true;
      log(`audit-log: open ${filePath} failed (${err instanceof Error ? err.message : String(err)}); falling back to stderr`);
    }
  }
  const sink: AuditSink = (entry) => {
    const line = `${JSON.stringify({ ts: new Date().toISOString(), pid, ...entry })}\n`;
    if (!latchedToStderr && stream !== undefined) {
      stream.write(line);
      return;
    }
    process.stderr.write(`audit: ${line}`);
  };
  const close = (): Promise<void> => {
    const s = stream;
    if (s === undefined) return Promise.resolve();
    return new Promise<void>((resolve) => {
      s.end(() => resolve());
    });
  };
  return { sink, close };
}
