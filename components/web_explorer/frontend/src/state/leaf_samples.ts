// === leaf_samples.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Var } from "@sen/client";
import { Quantity, Variant } from "@sen/client";

export type SampleValue = number | boolean | string;

// Locked at the first sample; later type-switches land as NaN / false / "" rather than
// rewriting history.
export type ValueKind = "number" | "boolean" | "string";

// Direct refs to the live typed arrays plus the valid-sample count. Consumers read only
// the first `size` indices and re-read on every invalidation; the arrays mutate in place
// as the ring overflows / trims.
//
// Pick the column whose vKind matches: vNum (numbers / Quantity values), vBool (0/1),
// vStr (enum / string). tServer[i] is NaN when the wire timestamp couldn't be parsed.
export interface BufferView {
  readonly size: number;
  readonly at: Float64Array;
  readonly tServer: Float64Array;
  readonly vKind: ValueKind | null;
  readonly vNum: Float64Array | null;
  readonly vBool: Uint8Array | null;
  readonly vStr: readonly string[] | null;
  // vGap[i] === 1 marks a leaf-absent sample (object removed, variant arm changed,
  // optional cleared). The value column holds a type-appropriate sentinel
  // (NaN / 0 / ""), but the gap flag is the source of truth. Null until the first sample.
  readonly vGap: Uint8Array | null;
  readonly unit: string | null;
}

const EMPTY_F64 = new Float64Array(0);

/** Stable reference for `useSyncExternalStore` when the leaf has no samples yet. */
export const EMPTY_VIEW: BufferView = Object.freeze({
  size: 0,
  at: EMPTY_F64,
  tServer: EMPTY_F64,
  vKind: null,
  vNum: null,
  vBool: null,
  vStr: null,
  vGap: null,
  unit: null,
});

/** Boxed read; tight loops should index the typed-array column directly. */
export function valueAt(view: BufferView, i: number): SampleValue {
  if (view.vNum !== null) return view.vNum[i] ?? 0;
  if (view.vBool !== null) return view.vBool[i] === 1;
  if (view.vStr !== null) return view.vStr[i] ?? "";
  return 0;
}

/** Server timestamp when parseable, local `at` otherwise. */
export function timeMsAt(view: BufferView, i: number): number {
  const ts = view.tServer[i] ?? NaN;
  return Number.isFinite(ts) ? ts : (view.at[i] ?? 0);
}


// Plottable = number, boolean, string, or the Variant tag. Quantity leaves carry the
// unit abbreviation; everything else has unit === null. Paths are dotted/bracketed
// (e.g. `position.x`, `samples[3].value`); root leaf is `""`.
export type SampleLeaf = readonly [path: string, value: SampleValue, unit: string | null];

export function sampleLeaves(v: Var, path: string): SampleLeaf[] {
  const out: SampleLeaf[] = [];
  pushSampleLeaves(v, path, out);
  return out;
}

function pushSampleLeaves(v: Var, path: string, out: SampleLeaf[]): void {
  if (typeof v === "number") {
    if (Number.isFinite(v)) out.push([path, v, null]);
    return;
  }
  if (typeof v === "boolean") {
    out.push([path, v, null]);
    return;
  }
  if (typeof v === "string") {
    out.push([path, v, null]);
    return;
  }
  if (v instanceof Quantity) {
    if (Number.isFinite(v.value)) {
      const u = v.unit;
      out.push([path, v.value, u.abbreviation || u.name || null]);
    }
    return;
  }
  if (v instanceof Variant) {
    // Emit the discriminator tag at this path; only recurse into payloads that produce
    // descendant paths. Primitives / Quantity / nested Variants would re-push at the
    // SAME path, which would land in the LeafBuffer we just locked to vKind=string and
    // corrupt it. Plotting "value inside arm X" isn't expressible here; user plots the tag.
    out.push([path, v.type, null]);
    const payload = v.value as Var;
    if (
      payload !== null &&
      typeof payload === "object" &&
      !(payload instanceof Quantity) &&
      !(payload instanceof Variant)
    ) {
      pushSampleLeaves(payload, path, out);
    }
    return;
  }
  if (Array.isArray(v)) {
    for (let i = 0; i < v.length; i++) {
      pushSampleLeaves(v[i]!, `${path}[${i}]`, out);
    }
    return;
  }
  if (v && typeof v === "object") {
    for (const [k, val] of Object.entries(v)) {
      pushSampleLeaves(val, path ? `${path}.${k}` : k, out);
    }
  }
}
