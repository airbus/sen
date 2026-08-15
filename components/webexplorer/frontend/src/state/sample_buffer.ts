// === sample_buffer.ts ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { PlotKey } from "../core/watch_keys.js";
import {
  EMPTY_VIEW,
  type BufferView,
  type SampleValue,
  type ValueKind,
} from "./leaf_samples.js";

// Parallel typed-array columns (~17 B/sample) instead of object-per-sample (~60-80 B);
// difference is tens vs hundreds of MB at typical session size. Value column type locks
// on the first sample; mismatched later samples land as NaN / false / "".

// Drop this fraction of capacity per eviction so push cost stays amortized O(1).
const SHIFT_CHUNK_FRACTION = 0.25;

export interface LeafBuffer {
  readonly capacity: number;
  size: number;
  readonly at: Float64Array;
  readonly tServer: Float64Array;
  vKind: ValueKind | null;
  vNum: Float64Array | null;
  vBool: Uint8Array | null;
  vStr: string[] | null;
  /** See `BufferView.vGap`. Lazy-allocated alongside the value column. */
  vGap: Uint8Array | null;
  unit: string | null;
  // Local-clock ms of the most recent push. Trim cycle drops a buffer with no fresh
  // pushes for ~2x retention and no UI subscribers; otherwise every once-watched leaf
  // holds ~100 KB of typed-array storage forever.
  lastPushAt: number;
  /** Cached so per-delivery invalidate() doesn't re-allocate the key string. */
  readonly plotKey: PlotKey;
}

export function makeBuffer(capacity: number, plotKey: PlotKey): LeafBuffer {
  return {
    capacity,
    size: 0,
    at: new Float64Array(capacity),
    tServer: new Float64Array(capacity),
    vKind: null,
    vNum: null,
    vBool: null,
    vStr: null,
    vGap: null,
    unit: null,
    lastPushAt: 0,
    plotKey,
  };
}

function ensureValueColumn(buf: LeafBuffer, v: SampleValue): void {
  if (buf.vKind !== null) return;
  if (typeof v === "number") {
    buf.vKind = "number";
    buf.vNum = new Float64Array(buf.capacity);
  } else if (typeof v === "boolean") {
    buf.vKind = "boolean";
    buf.vBool = new Uint8Array(buf.capacity);
  } else {
    buf.vKind = "string";
    buf.vStr = [];
  }
  buf.vGap = new Uint8Array(buf.capacity);
}

export function pushToBuffer(
  buf: LeafBuffer,
  at: number,
  tServer: number | null,
  v: SampleValue,
  unit: string | null,
): void {
  ensureValueColumn(buf, v);
  shiftIfFull(buf);
  const i = buf.size;
  buf.at[i] = at;
  buf.tServer[i] = tServer ?? NaN;
  if (buf.vNum) {
    buf.vNum[i] = typeof v === "number" ? v : NaN;
  } else if (buf.vBool) {
    buf.vBool[i] = typeof v === "boolean" && v ? 1 : 0;
  } else if (buf.vStr) {
    buf.vStr.push(typeof v === "string" ? v : String(v));
  }
  if (buf.vGap) buf.vGap[i] = 0;
  if (unit !== null) buf.unit = unit;
  buf.size++;
  buf.lastPushAt = at;
}

// Marks the leaf as absent at `at`. Idempotent: no-op when the buffer is empty or the
// last sample is already a sentinel. The value column gets a sensible zero; the vGap
// flag is the source of truth. Teardown call sites (no incoming delivery) pass
// tServer=null; otherwise pass the server timestamp so the gap anchors to the same
// clock as neighboring real samples.
export function pushSentinelToBuffer(
  buf: LeafBuffer,
  at: number,
  tServer: number | null,
): boolean {
  if (buf.size === 0 || buf.vKind === null || buf.vGap === null) return false;
  const lastIdx = buf.size - 1;
  if (buf.vGap[lastIdx] === 1) return false;
  shiftIfFull(buf);
  const i = buf.size;
  buf.at[i] = at;
  buf.tServer[i] = tServer ?? NaN;
  if (buf.vNum) buf.vNum[i] = NaN;
  else if (buf.vBool) buf.vBool[i] = 0;
  else if (buf.vStr) buf.vStr.push("");
  buf.vGap[i] = 1;
  buf.size++;
  buf.lastPushAt = at;
  return true;
}

function shiftIfFull(buf: LeafBuffer): void {
  if (buf.size < buf.capacity) return;
  const shift = Math.max(1, Math.floor(buf.capacity * SHIFT_CHUNK_FRACTION));
  buf.at.copyWithin(0, shift, buf.size);
  buf.tServer.copyWithin(0, shift, buf.size);
  if (buf.vNum) buf.vNum.copyWithin(0, shift, buf.size);
  if (buf.vBool) buf.vBool.copyWithin(0, shift, buf.size);
  if (buf.vStr) buf.vStr.splice(0, shift);
  if (buf.vGap) buf.vGap.copyWithin(0, shift, buf.size);
  buf.size -= shift;
}

// Keeps at least one sample so a quiet property still shows its current value past retention.
export function trimBuffer(buf: LeafBuffer, cutoff: number): boolean {
  const maxDrop = buf.size - 1;
  let drop = 0;
  while (drop < maxDrop && buf.at[drop]! < cutoff) drop++;
  if (drop === 0) return false;
  buf.at.copyWithin(0, drop, buf.size);
  buf.tServer.copyWithin(0, drop, buf.size);
  if (buf.vNum) buf.vNum.copyWithin(0, drop, buf.size);
  if (buf.vBool) buf.vBool.copyWithin(0, drop, buf.size);
  if (buf.vStr) buf.vStr.splice(0, drop);
  if (buf.vGap) buf.vGap.copyWithin(0, drop, buf.size);
  buf.size -= drop;
  return true;
}

/** Typed-array refs are shared with the live buffer. */
export function viewOfBuffer(buf: LeafBuffer): BufferView {
  if (buf.size === 0) return EMPTY_VIEW;
  return {
    size: buf.size,
    at: buf.at,
    tServer: buf.tServer,
    vKind: buf.vKind,
    vNum: buf.vNum,
    vBool: buf.vBool,
    vStr: buf.vStr,
    vGap: buf.vGap,
    unit: buf.unit,
  };
}
