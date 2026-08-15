// === FieldWriter.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useRef, useState } from "react";
import type * as React from "react";

import type { Client, ObjectHandle, Var } from "@sen/client";
import { Quantity, Variant } from "@sen/client";

import { defaultFor, isComposite } from "../../core/types.js";
import { CheckIcon } from "../../ui/icons.js";
import { Muted } from "../../ui/text_primitives.js";
import { chipButton, truncate } from "./_styles.js";
import { ValueInput } from "./value_input.js";

export interface FieldWriterProps {
  client: Client | null;
  obj: ObjectHandle;
  propertyName: string;
  declaredType: string;
  // Live server value; drives the buffer when not held.
  value: Var | undefined;
}

export function FieldWriter({ client, obj, propertyName, declaredType, value }: FieldWriterProps) {
  const editor = useFieldEditor<Var>({
    value: value ?? defaultFor(client, declaredType),
    write: async (next) => {
      await obj.set(propertyName, next);
    },
  });
  if (value === undefined) {
    return <Muted>--</Muted>;
  }
  // Composites use chips only; their nested inputs would conflict with Enter/Esc here.
  const allowKeyShortcuts = !isComposite(client, declaredType);
  const onKeyDown = allowKeyShortcuts
    ? (e: React.KeyboardEvent<HTMLDivElement>) => {
        if (e.key === "Enter" && editor.state.status === "dirty") {
          e.preventDefault();
          void editor.apply();
        } else if (e.key === "Escape") {
          e.preventDefault();
          editor.cancel();
        }
      }
    : undefined;
  return (
    <div
      onKeyDown={onKeyDown}
      style={{ display: "flex", alignItems: "center", gap: 8, flexWrap: "wrap" }}
    >
      <ValueInput
        client={client}
        declaredType={declaredType}
        value={editor.state.buffer}
        onChange={editor.setBuffer}
      />
      <FieldChrome state={editor.state} apply={editor.apply} cancel={editor.cancel} />
    </div>
  );
}

function FieldChrome({
  state,
  apply,
  cancel,
}: {
  state: EditorState<Var>;
  apply: () => Promise<void>;
  cancel: () => void;
}) {
  return (
    <span style={{ display: "inline-flex", alignItems: "center", gap: 6 }}>
      {state.status === "dirty" && (
        <>
          <button type="button" onClick={() => void apply()} style={chipButton("accent")}>
            Apply
          </button>
          <button type="button" onClick={cancel} style={chipButton("ghost")}>
            Cancel
          </button>
          <span style={{ fontSize: "var(--fs-sm)", color: "var(--warn)" }}>held</span>
        </>
      )}
      {state.status === "pending" && (
        <span style={{ fontSize: "var(--fs-sm)", color: "var(--fg-muted)" }}>writing...</span>
      )}
      {state.status === "ok" && (
        <span
          style={{ color: "var(--ok)", display: "inline-flex", alignItems: "center" }}
          aria-label="ok"
          title="Applied"
        >
          <CheckIcon />
        </span>
      )}
      {state.status === "err" && state.error && (
        <span
          style={{
            fontSize: "var(--fs-sm)",
            color: "var(--err)",
            fontFamily: "var(--font-mono)",
            maxWidth: 360,
          }}
          title={state.error.message}
        >
          rejected: {truncate(state.error.message, 80)}
        </span>
      )}
    </span>
  );
}

export type EditorStatus = "clean" | "dirty" | "pending" | "ok" | "err";

export interface EditorState<T> {
  status: EditorStatus;
  buffer: T;
  held: boolean;
  error: Error | null;
}

interface UseFieldEditorArgs<T> {
  value: T;
  write: (next: T) => Promise<void>;
  equals?: (a: T, b: T) => boolean;
}

interface UseFieldEditorResult<T> {
  state: EditorState<T>;
  setBuffer: (next: T) => void;
  apply: () => Promise<void>;
  cancel: () => void;
}

// Walks the runtime shape with a depth cap; no serialization.
export function defaultEquals(a: unknown, b: unknown): boolean {
  return varEquals(a, b, 0);
}

const MAX_EQUALS_DEPTH = 32;

function varEquals(a: unknown, b: unknown, depth: number): boolean {
  if (a === b) return true;
  if (a === null || a === undefined || b === null || b === undefined) return false;
  if (depth > MAX_EQUALS_DEPTH) return false;
  if (a instanceof Quantity) {
    if (!(b instanceof Quantity)) return false;
    return a.value === b.value && a.unit.name === b.unit.name;
  }
  if (b instanceof Quantity) return false;
  if (a instanceof Variant) {
    if (!(b instanceof Variant)) return false;
    return a.type === b.type && varEquals(a.value, b.value, depth + 1);
  }
  if (b instanceof Variant) return false;
  if (Array.isArray(a)) {
    if (!Array.isArray(b) || a.length !== b.length) return false;
    for (let i = 0; i < a.length; i++) {
      if (!varEquals(a[i], b[i], depth + 1)) return false;
    }
    return true;
  }
  if (Array.isArray(b)) return false;
  if (typeof a === "object" && typeof b === "object") {
    const ka = Object.keys(a as Record<string, unknown>);
    const kb = Object.keys(b as Record<string, unknown>);
    if (ka.length !== kb.length) return false;
    for (const k of ka) {
      if (!Object.prototype.hasOwnProperty.call(b, k)) return false;
      if (!varEquals((a as Record<string, unknown>)[k], (b as Record<string, unknown>)[k], depth + 1)) return false;
    }
    return true;
  }
  return false;
}

function useFieldEditor<T>({
  value,
  write,
  equals = defaultEquals as (a: T, b: T) => boolean,
}: UseFieldEditorArgs<T>): UseFieldEditorResult<T> {
  const [state, setState] = useState<EditorState<T>>(() => ({
    status: "clean",
    buffer: value,
    held: false,
    error: null,
  }));
  const heldRef = useRef(false);
  heldRef.current = state.held;
  // Read at error/cancel time so a value-update racing the write doesn't restore stale data.
  const latestValueRef = useRef(value);
  latestValueRef.current = value;
  // Keep apply() identity-stable across keystrokes; the ref reads the buffer at call time.
  const bufferRef = useRef(state.buffer);
  bufferRef.current = state.buffer;
  // Cancel on unmount so the flash setTimeout can't fire setState on a dead component.
  const flashTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => {
    return () => {
      if (flashTimerRef.current !== null) clearTimeout(flashTimerRef.current);
    };
  }, []);

  useEffect(() => {
    if (heldRef.current) return;
    setState((prev) =>
      equals(prev.buffer, value) ? prev : { ...prev, buffer: value, status: "clean", error: null },
    );
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [value]);

  const setBuffer = useCallback(
    (next: T) => {
      setState(() => {
        const dirty = !equals(next, value);
        return {
          status: dirty ? "dirty" : "clean",
          buffer: next,
          held: dirty,
          error: null,
        };
      });
    },
    [value, equals],
  );

  const cancel = useCallback(() => {
    setState({ status: "clean", buffer: latestValueRef.current, held: false, error: null });
  }, []);

  const apply = useCallback(async () => {
    const buf = bufferRef.current;
    setState((prev) => ({ ...prev, status: "pending", error: null }));
    try {
      await write(buf);
      setState({ status: "ok", buffer: buf, held: false, error: null });
      if (flashTimerRef.current !== null) clearTimeout(flashTimerRef.current);
      flashTimerRef.current = setTimeout(() => {
        flashTimerRef.current = null;
        setState((prev) => (prev.status === "ok" ? { ...prev, status: "clean" } : prev));
      }, 800);
    } catch (err) {
      const e = err instanceof Error ? err : new Error(String(err));
      setState({ status: "err", buffer: latestValueRef.current, held: false, error: e });
    }
  }, [write]);

  return { state, setBuffer, apply, cancel };
}
