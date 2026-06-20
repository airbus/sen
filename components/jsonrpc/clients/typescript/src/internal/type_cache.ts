// === type_cache.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { TransportError } from "../errors.js";
import { defaultReportError } from "./report_error.js";
import type { CustomTypeSpec, CustomTypeSpecList } from "../generated/index.js";
import type { ReportError } from "../connect.js";

/**
 * Type-spec store for the Var parser. Filled from `interestUpdate.types` before any handle
 * needs it, so consumers read it synchronously. Observers can subscribe via {@link subscribe}
 * to be notified when the cache grows; the Client exposes this through `Client.onTypeAdded`.
 *
 * `derived` is a per-cache scratch space the Var codec uses for memoized lookups that depend
 * on this cache's contents (e.g., flattened struct field lists). Per-cache because the result
 * of those lookups varies with what's in this cache; a shared global WeakMap would leak stale
 * results across caches with different parent sets.
 */
export class TypeCache {
  private readonly cache = new Map<string, CustomTypeSpec>();
  private readonly schemas = new Map<string, object>();
  private readonly listeners = new Set<() => void>();
  public readonly derived = new WeakMap<object, unknown>();

  constructor(private readonly reportError: ReportError = defaultReportError) {}

  get(qualifiedName: string): CustomTypeSpec | undefined {
    return this.cache.get(qualifiedName);
  }

  /** JSON-Schema fragment for a qualified name, or undefined when the wire didn't ship one
   *  (interest opened without `withSchemas` / type fetched without `withSchema`). */
  getSchema(qualifiedName: string): object | undefined {
    return this.schemas.get(qualifiedName);
  }

  /** Stores one schema fragment. Does not fire listeners on its own; schemas always arrive
   *  alongside specs, so the spec-side `fire()` already covers observers. */
  setSchema(qualifiedName: string, fragment: object): void {
    this.schemas.set(qualifiedName, fragment);
  }

  /** Decodes the `typeSchemas` string from an `interestUpdate` (a JSON-encoded map keyed by
   *  qualified name) and stores every fragment. Empty strings are no-ops. Malformed JSON is
   *  surfaced via reportError rather than thrown: a bad schema must never break the wire-
   *  delivery path, but a misbehaving server should not silently lose every schema either. */
  setSchemasFromBlob(blob: string): void {
    if (blob.length === 0) return;
    try {
      const parsed = JSON.parse(blob) as Record<string, object>;
      for (const [qual, fragment] of Object.entries(parsed)) {
        this.schemas.set(qual, fragment);
      }
    } catch (err) {
      this.reportError(
        new TransportError("interestUpdate.typeSchemas blob failed to parse as JSON", {
          cause: err,
        }),
      );
    }
  }

  set(spec: CustomTypeSpec): void {
    const present = this.cache.has(spec.qualifiedName);
    this.cache.set(spec.qualifiedName, spec);
    if (!present) this.fire();
  }

  setMany(specs: CustomTypeSpecList): void {
    if (specs.length === 0) return;
    let added = false;
    for (const spec of specs) {
      if (!this.cache.has(spec.qualifiedName)) added = true;
      this.cache.set(spec.qualifiedName, spec);
    }
    if (added) this.fire();
  }

  has(qualifiedName: string): boolean {
    return this.cache.has(qualifiedName);
  }

  size(): number {
    return this.cache.size;
  }

  values(): CustomTypeSpec[] {
    return Array.from(this.cache.values());
  }

  clear(): void {
    this.cache.clear();
  }

  /** Drop every subscriber. Called from `Client.close()` so listeners registered against
   *  a closed client are released even when their owning React components stay mounted. */
  clearListeners(): void {
    this.listeners.clear();
  }

  /** Subscribe to cache growth. Fires once per batch in which at least one previously-unseen
   *  type name was added; existing entries being overwritten don't fire. Returns an unsubscribe
   *  function. Listeners that throw have their exception swallowed: an observer must never
   *  break the wire-delivery path that fed the cache. */
  subscribe(listener: () => void): () => void {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  }

  private fire(): void {
    for (const listener of this.listeners) {
      try {
        listener();
      } catch {
        /* observer threw; ignore so the wire handler stays alive */
      }
    }
  }
}
