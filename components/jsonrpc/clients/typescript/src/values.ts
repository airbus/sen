// === values.ts =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { UnitInfo } from "./generated/index.js";

/** Cancel function returned by subscribe-shaped APIs. Idempotent. */
export type CancelFn = () => void;

/**
 * A numeric value paired with its unit. `minValue` and `maxValue` come from the type spec
 * (the STL definition) and may be `null` if unconstrained.
 *
 * @example
 * ```ts
 * const altitude = await aircraft.get("altitude");
 * if (isQuantity(altitude)) {
 *   console.log(`${altitude.value} ${altitude.unit.abbreviation}`);
 * }
 * ```
 */
export class Quantity {
  constructor(
    public readonly value: number,
    public readonly unit: UnitInfo,
    public readonly minValue: number | null = null,
    public readonly maxValue: number | null = null,
  ) {}

  /** Render as `<value> <abbreviation>` (e.g. `"100 m"`), or just `<value>` if no abbreviation. */
  toString(): string {
    return this.unit.abbreviation ? `${this.value} ${this.unit.abbreviation}` : String(this.value);
  }

  /** Structural equality across value, unit, and bounds. */
  equals(other: Quantity): boolean {
    return (
      this.value === other.value &&
      this.unit.name === other.unit.name &&
      this.unit.abbreviation === other.unit.abbreviation &&
      this.unit.category === other.unit.category &&
      this.minValue === other.minValue &&
      this.maxValue === other.maxValue
    );
  }
}

/**
 * A tagged union value: pick which arm with `type`, the payload is `value`. Kept distinct from
 * a plain struct so `instanceof` narrowing works and code can tell `{type, value}` payloads
 * apart from structs that happen to have those field names.
 *
 * @example
 * ```ts
 * const status = await student.get("status");
 * if (isVariant(status)) {
 *   if (status.type === "Sleeping") console.log("zzz");
 * }
 * ```
 */
export class Variant<T = unknown> {
  constructor(
    public readonly type: string,
    public readonly value: T,
  ) {}
}

/**
 * Any value the library returns (property reads, event args, method results). TypeScript analogue
 * of C++ `sen::Var`. Enums appear as bare strings; `i64` / `u64` properties appear as `bigint`
 * (they cross the wire as decimal strings to preserve precision past `Number.MAX_SAFE_INTEGER`).
 */
export type Var =
  | string
  | number
  | bigint
  | boolean
  | null
  | Var[]
  | { [field: string]: Var }
  | Variant
  | Quantity;

/**
 * Narrow a `bigint` (typically from a `u64` / `i64` property) down to a `number`, throwing
 * `RangeError` if the value would lose precision (outside `[Number.MIN_SAFE_INTEGER,
 * MAX_SAFE_INTEGER]`). `context` is included in the error message.
 *
 * Use at boundaries where downstream APIs need a `number` (uPlot x-axes, `Date(ms)`,
 * ms-range counters). Implicit `bigint -> number` is already a TS type error; this is the
 * loud explicit downgrade.
 */
export function numberFromExact(value: bigint, context?: string): number {
  if (value > BigInt(Number.MAX_SAFE_INTEGER) || value < BigInt(Number.MIN_SAFE_INTEGER)) {
    const where = context ? ` (${context})` : "";
    throw new RangeError(`Cannot represent ${value} as a safe Number${where}`);
  }
  return Number(value);
}

/** Sequence of `Var`s. */
export type VarList = Var[];

/** String-keyed map of `Var`s. */
export type VarMap = { [field: string]: Var };
