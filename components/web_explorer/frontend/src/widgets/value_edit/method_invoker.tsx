// === MethodInvoker.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useState } from "react";
import type * as React from "react";

import type { Client, ObjectHandle, Var } from "@sen/client";

import { defaultFor } from "../../core/types.js";
import { Collapsible } from "../../ui/layout.js";
import { HelpButton, HoverIconButton } from "../../ui/buttons.js";

import { Tooltip } from "../../ui/tooltip.js";
import { TypeChip } from "../../ui/chips.js";
import {
  CheckIcon,
  ChevronDownIcon,
  ChevronRightIcon,
  CloseIcon,
  InvokeIcon,
} from "../../ui/icons.js";
import { ValueRenderer } from "../value_render/value_renderer.js";
import { isVoidReturn } from "./_styles.js";
import { ValueInput } from "./value_input.js";

export interface MethodArgInfo {
  name: string;
  description: string;
  type: string;
}

export interface MethodSpecLite {
  name: string;
  description: string;
  args: MethodArgInfo[];
  returnType: string;
}

export interface MethodInvokerProps {
  client: Client | null;
  obj: ObjectHandle;
  method: MethodSpecLite;
  // Lifted so the methods tab can enforce accordion behavior across siblings.
  expanded: boolean;
  onExpandedChange: (next: boolean) => void;
}

interface Invocation {
  id: number;
  argsSnapshot: Record<string, Var>;
  status: "pending" | "ok" | "err";
  result?: Var;
  error?: Error;
}

// Cap rendered invocation rows; pending entries are exempt (they have nowhere to land).
const MAX_INVOCATIONS = 10;

export function MethodInvoker({
  client,
  obj,
  method,
  expanded,
  onExpandedChange,
}: MethodInvokerProps) {
  const [args, setArgs] = useState<Record<string, Var>>(() =>
    Object.fromEntries(
      method.args.map((a) => [a.name, defaultFor(client, a.type)]),
    ),
  );
  const [invocations, setInvocations] = useState<Invocation[]>([]);
  const [nextInvocationId, setNextInvocationId] = useState(1);

  const invoke = async () => {
    const id = nextInvocationId;
    setNextInvocationId((n) => n + 1);
    // Snapshot args so the row reflects the call made, not later edits.
    const argsSnapshot: Record<string, Var> = { ...args };
    setInvocations((prev) => prependCapped({
      id,
      argsSnapshot,
      status: "pending",
    }, prev));
    try {
      const r = await obj.invoke(method.name, argsSnapshot);
      setInvocations((prev) =>
        prev.map((inv) => (inv.id === id ? { ...inv, status: "ok", result: r } : inv)),
      );
    } catch (err) {
      const error = err instanceof Error ? err : new Error(String(err));
      setInvocations((prev) =>
        prev.map((inv) => (inv.id === id ? { ...inv, status: "err", error } : inv)),
      );
    }
  };
  const dismissInvocation = (id: number) => {
    setInvocations((prev) => prev.filter((inv) => inv.id !== id));
  };
  const clearSettled = () => {
    setInvocations((prev) => prev.filter((inv) => inv.status === "pending"));
  };
  const pendingCount = invocations.filter((inv) => inv.status === "pending").length;
  const settledCount = invocations.length - pendingCount;

  const hasArgs = method.args.length > 0;
  // Collapsed + has args: Invoke is dim and expands rather than calls.
  const invokeDim = hasArgs && !expanded;
  const invokeTooltip =
    hasArgs && !expanded ? "Expand to set arguments" : "Invoke";
  const onInvokeClick = () => {
    if (hasArgs && !expanded) {
      onExpandedChange(true);
      return;
    }
    void invoke();
  };
  const showFeedback = (!hasArgs || expanded) && invocations.length > 0;

  const toggleExpand = () => {
    if (hasArgs) onExpandedChange(!expanded);
  };
  const onRowKeyDown = (e: React.KeyboardEvent<HTMLDivElement>) => {
    if (!hasArgs) return;
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      toggleExpand();
    }
  };
  // Stop propagation so inner-row buttons don't also toggle expand.
  const stop = (e: React.MouseEvent | React.KeyboardEvent) => e.stopPropagation();

  return (
    <div
      className="carved-bottom"
      style={{
        display: "flex",
        alignItems: "stretch",
      }}
    >
      <div style={{ flex: 1, minWidth: 0 }}>
        <div
          className="property-row"
          role={hasArgs ? "button" : undefined}
          tabIndex={hasArgs ? 0 : undefined}
          aria-expanded={hasArgs ? expanded : undefined}
          onClick={toggleExpand}
          onKeyDown={onRowKeyDown}
          style={{
            display: "flex",
            alignItems: "center",
            gap: 8,
            padding: "5px 10px",
            minHeight: 28,
            cursor: hasArgs ? "pointer" : "default",
          }}
        >
          {hasArgs ? (
            <span
              style={{
                display: "inline-flex",
                alignItems: "center",
                justifyContent: "center",
                width: 16,
                color: "var(--fg-muted)",
                flex: "none",
              }}
            >
              {expanded ? <ChevronDownIcon /> : <ChevronRightIcon />}
            </span>
          ) : (
            // Spacer matches chevron width so names align across rows with/without args.
            <span style={{ display: "inline-block", width: 16, flex: "none" }} />
          )}
          <span
            style={{
              fontFamily: "var(--font-mono)",
              fontSize: "var(--fs-lg)",
              color: "var(--fg-base)",
              flex: "none",
            }}
          >
            {method.name}
          </span>
          {pendingCount > 0 && (
            <span
              style={{
                marginLeft: "auto",
                display: "inline-flex",
                alignItems: "center",
                fontSize: "var(--fs-sm)",
                color: "var(--fg-muted)",
              }}
            >
              {pendingCount === 1 ? "calling..." : `${pendingCount} in flight...`}
            </span>
          )}
          <span
            onClick={stop}
            onKeyDown={stop}
            style={{ display: "inline-flex", flex: "none", marginLeft: pendingCount > 0 ? 0 : "auto" }}
          >
            <HelpButton
              description={method.description}
              ariaLabel={`${method.name} documentation`}
            />
          </span>
        </div>
        {hasArgs && (
          <Collapsible open={expanded}>
            <div
              style={{
                padding: "4px 10px 10px 34px",
                display: "flex",
                flexDirection: "column",
                gap: 6,
              }}
            >
              {method.args.map((arg) => (
                <div key={arg.name} style={{ display: "flex", alignItems: "baseline", gap: 8, flexWrap: "wrap" }}>
                  <Tooltip content={arg.description}>
                    <span
                      style={{
                        display: "inline-flex",
                        flexDirection: "column",
                        alignItems: "flex-start",
                        gap: 2,
                        minWidth: 80,
                      }}
                    >
                      <span
                        style={{
                          fontFamily: "var(--font-mono)",
                          fontSize: "var(--fs-md)",
                          color: "var(--fg-muted)",
                        }}
                      >
                        {arg.name}
                      </span>
                      <TypeChip client={client} type={arg.type} />
                    </span>
                  </Tooltip>
                  <ValueInput
                    client={client}
                    declaredType={arg.type}
                    value={args[arg.name] ?? defaultFor(client, arg.type)}
                    onChange={(next) => setArgs((prev) => ({ ...prev, [arg.name]: next }))}
                  />
                </div>
              ))}
            </div>
          </Collapsible>
        )}
        {showFeedback && (
          <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
            {settledCount > 1 && (
              <div
                style={{
                  display: "flex",
                  justifyContent: "flex-end",
                  padding: "0 10px",
                }}
              >
                <HoverIconButton
                  onClick={clearSettled}
                  ariaLabel="dismiss all settled invocations"
                  tooltip="Clear settled"
                  icon={<CloseIcon />}
                  size={20}
                />
              </div>
            )}
            {invocations.map((inv) => (
              <InvocationRow
                key={inv.id}
                client={client}
                returnType={method.returnType}
                invocation={inv}
                onDismiss={() => dismissInvocation(inv.id)}
              />
            ))}
          </div>
        )}
      </div>
      <span onClick={stop} onKeyDown={stop} style={{ display: "flex" }}>
        <MethodRowInvokeButton
          onClick={onInvokeClick}
          dim={invokeDim}
          tooltip={invokeTooltip}
          methodName={method.name}
        />
      </span>
    </div>
  );
}

function InvocationRow({
  client,
  returnType,
  invocation,
  onDismiss,
}: {
  client: Client | null;
  returnType: string;
  invocation: Invocation;
  onDismiss: () => void;
}) {
  const isPending = invocation.status === "pending";
  const isError = invocation.status === "err";
  const borderColor = isError
    ? "var(--err)"
    : isPending
      ? "var(--border-muted, var(--border-default))"
      : "var(--border-default)";
  return (
    <div
      style={{
        margin: "0 10px 0 34px",
        padding: "6px 8px",
        background: "var(--bg-elevated)",
        border: `1px solid ${borderColor}`,
        borderRadius: "var(--radius-sm)",
        display: "flex",
        alignItems: "center",
        gap: 8,
        flexWrap: "wrap",
        color: isError ? "var(--err)" : undefined,
        opacity: isPending ? 0.85 : 1,
        transition: "opacity 120ms ease, border-color 120ms ease",
      }}
    >
      <InvocationStatusIcon status={invocation.status} />
      <InvocationArgsSummary argsSnapshot={invocation.argsSnapshot} />
      {invocation.status === "ok" && !isVoidReturn(returnType) && (
        <>
          <span
            style={{
              fontSize: "var(--fs-xs)",
              color: "var(--fg-subtle)",
              textTransform: "uppercase",
              letterSpacing: "0.05em",
              fontFamily: "var(--font-mono)",
            }}
          >
            returned
          </span>
          <ValueRenderer client={client} declaredType={returnType} value={invocation.result} />
        </>
      )}
      {invocation.status === "err" && invocation.error && (
        <span
          style={{
            flex: 1,
            minWidth: 0,
            whiteSpace: "pre-wrap",
            wordBreak: "break-word",
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
          }}
        >
          {invocation.error.message}
        </span>
      )}
      {!isPending && (
        <span style={{ marginLeft: "auto", display: "inline-flex" }}>
          <HoverIconButton
            onClick={onDismiss}
            ariaLabel={isError ? "dismiss error" : "dismiss result"}
            tooltip="Dismiss"
            icon={<CloseIcon />}
            size={20}
            danger={isError}
          />
        </span>
      )}
    </div>
  );
}

function InvocationStatusIcon({ status }: { status: Invocation["status"] }) {
  if (status === "pending") {
    return (
      <span
        aria-label="pending"
        title="Awaiting response"
        style={{
          display: "inline-block",
          width: 14,
          height: 14,
          border: "2px solid var(--border-default)",
          borderTopColor: "var(--accent)",
          borderRadius: "50%",
          animation: "sen-invocation-spin 0.9s linear infinite",
          flex: "none",
        }}
      />
    );
  }
  if (status === "ok") {
    return (
      <span
        style={{ color: "var(--ok)", display: "inline-flex", alignItems: "center" }}
        aria-label="ok"
        title="Invoked successfully"
      >
        <CheckIcon />
      </span>
    );
  }
  return (
    <span
      style={{ color: "var(--err)", display: "inline-flex", alignItems: "center" }}
      aria-label="error"
      title="Invocation failed"
    >
      <CloseIcon />
    </span>
  );
}

function InvocationArgsSummary({ argsSnapshot }: { argsSnapshot: Record<string, Var> }) {
  const names = Object.keys(argsSnapshot);
  if (names.length === 0) return null;
  return (
    <span
      style={{
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-sm)",
        color: "var(--fg-muted)",
        whiteSpace: "nowrap",
        overflow: "hidden",
        textOverflow: "ellipsis",
        maxWidth: "30ch",
      }}
      title={names.map((n) => `${n}: ${formatArgPreview(argsSnapshot[n])}`).join(", ")}
    >
      {names.map((n) => `${n}: ${formatArgPreview(argsSnapshot[n])}`).join(", ")}
    </span>
  );
}

function formatArgPreview(value: Var | undefined): string {
  if (value === null || value === undefined) return "null";
  if (typeof value === "string") return JSON.stringify(value);
  if (typeof value === "number" || typeof value === "boolean") return String(value);
  try {
    return JSON.stringify(value);
  } catch {
    return "...";
  }
}

// Newest-first; evict oldest settled past MAX_INVOCATIONS, never pending.
function prependCapped(next: Invocation, prev: Invocation[]): Invocation[] {
  const head = [next, ...prev];
  if (head.length <= MAX_INVOCATIONS) return head;
  let overflow = head.length - MAX_INVOCATIONS;
  return head.filter((inv) => {
    if (overflow <= 0) return true;
    if (inv.status === "pending") return true;
    overflow -= 1;
    return false;
  });
}

// dim is visual only; the button stays clickable so the parent can route to expand vs call.
function MethodRowInvokeButton({
  onClick,
  dim,
  tooltip,
  methodName,
}: {
  onClick: () => void;
  dim: boolean;
  tooltip: string;
  methodName: string;
}) {
  const [hover, setHover] = useState(false);
  const iconColor = dim ? "var(--fg-faint)" : "var(--accent)";
  return (
    <button
      type="button"
      onClick={onClick}
      title={tooltip}
      aria-label={`invoke ${methodName}`}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        width: 28,
        height: 28,
        padding: 0,
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        border: `1px solid ${hover ? "var(--accent)" : "transparent"}`,
        borderRadius: "var(--radius-sm)",
        background: hover ? "var(--accent-wash)" : "transparent",
        color: hover ? "var(--accent-text-wash)" : iconColor,
        cursor: "pointer",
        opacity: dim ? 0.55 : 1,
        flex: "none",
        transition:
          "background 120ms ease, color 120ms ease, border-color 120ms ease, opacity 120ms ease",
      }}
    >
      <InvokeIcon />
    </button>
  );
}
