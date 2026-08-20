// === AddInterestPopover.tsx ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { forwardRef, useEffect, useRef, useState, type ReactNode } from "react";
import type * as React from "react";

import type { Client } from "@sen/client";

import { Modal } from "../../ui/modal.js";
import { ClassSuggestions, useClassMatches } from "./popover/class_suggestions.js";
import { ModeToggle } from "./popover/mode_toggle.js";
import { useClassProperties } from "./popover/sql_helpers.js";
import { useInterestValidation } from "./popover/use_interest_validation.js";
import { useQueryAuthoring } from "./popover/use_query_authoring.js";
import { useWhereCaretCompletion } from "./popover/use_where_caret_completion.js";
import { useWhereMatches, WhereGuidance } from "./popover/where_guidance.js";

function themedInputStyle(focused: boolean, extra: React.CSSProperties = {}): React.CSSProperties {
  return {
    width: "100%",
    background: "var(--bg-input)",
    border: `1px solid ${focused ? "var(--accent)" : "var(--border-default)"}`,
    borderRadius: "var(--radius-sm)",
    color: "var(--fg-base)",
    fontFamily: "var(--font-mono)",
    fontSize: "var(--fs-md)",
    padding: "4px 6px",
    outline: "none",
    boxSizing: "border-box",
    boxShadow: focused ? "0 0 0 2px var(--accent-wash)" : "none",
    transition: "border-color 120ms ease, box-shadow 120ms ease",
    ...extra,
  };
}

type ThemedInputProps = Omit<React.InputHTMLAttributes<HTMLInputElement>, "style"> & {
  extraStyle?: React.CSSProperties;
};

const ThemedInput = forwardRef<HTMLInputElement, ThemedInputProps>(
  ({ extraStyle, onFocus, onBlur, ...rest }, ref) => {
    const [focused, setFocused] = useState(false);
    return (
      <input
        ref={ref}
        {...rest}
        onFocus={(e) => {
          setFocused(true);
          onFocus?.(e);
        }}
        onBlur={(e) => {
          setFocused(false);
          onBlur?.(e);
        }}
        style={themedInputStyle(focused, extraStyle)}
      />
    );
  },
);
ThemedInput.displayName = "ThemedInput";

type ThemedTextareaProps = Omit<React.TextareaHTMLAttributes<HTMLTextAreaElement>, "style"> & {
  extraStyle?: React.CSSProperties;
};

const ThemedTextarea = forwardRef<HTMLTextAreaElement, ThemedTextareaProps>(
  ({ extraStyle, onFocus, onBlur, ...rest }, ref) => {
    const [focused, setFocused] = useState(false);
    return (
      <textarea
        ref={ref}
        {...rest}
        onFocus={(e) => {
          setFocused(true);
          onFocus?.(e);
        }}
        onBlur={(e) => {
          setFocused(false);
          onBlur?.(e);
        }}
        style={themedInputStyle(focused, extraStyle)}
      />
    );
  },
);
ThemedTextarea.displayName = "ThemedTextarea";

export function AddInterestPopover({
  client,
  sessionName,
  busName,
  existingNames,
  onSave,
  onCancel,
}: {
  client: Client;
  sessionName: string;
  busName: string;
  existingNames: string[];
  onSave: (name: string, query: string) => void;
  onCancel: () => void;
}) {
  const authoring = useQueryAuthoring({ sessionName, busName, existingNames });
  const { error, submitting, validate, abort } = useInterestValidation({ client, existingNames });
  const properties = useClassProperties(client, authoring.classSpec.trim());
  const nameRef = useRef<HTMLInputElement | null>(null);
  const whereRef = useRef<HTMLInputElement | null>(null);
  const completion = useWhereCaretCompletion(authoring.whereClause, authoring.setWhereClause, whereRef);

  // -1 = no chip highlighted (Tab moves focus normally); 0..n-1 = chip index Tab inserts.
  // Resets on typing/caret motion so the highlight can't lag behind input.
  const classMatchesInfo = useClassMatches(client, authoring.classSpec);
  const whereMatchesInfo = useWhereMatches(properties, completion.currentWord.word);
  const [classFocus, setClassFocus] = useState(-1);
  const [whereFocus, setWhereFocus] = useState(-1);
  useEffect(() => setClassFocus(-1), [authoring.classSpec]);
  useEffect(() => setWhereFocus(-1), [completion.currentWord.word, completion.currentWord.start]);

  // Returns true when the event was consumed; false to let the input handle it natively.
  const navigateSuggestions = (
    e: React.KeyboardEvent<HTMLElement>,
    focusedIdx: number,
    setFocusedIdx: (n: number) => void,
    count: number,
    onAccept: (idx: number) => void,
  ): boolean => {
    if (count === 0) return false;
    if (e.key === "ArrowDown") {
      e.preventDefault();
      setFocusedIdx(focusedIdx + 1 >= count ? 0 : focusedIdx + 1);
      return true;
    }
    if (e.key === "ArrowUp") {
      e.preventDefault();
      setFocusedIdx(focusedIdx <= 0 ? count - 1 : focusedIdx - 1);
      return true;
    }
    if (e.key === "Tab" && !e.shiftKey && focusedIdx >= 0) {
      e.preventDefault();
      onAccept(focusedIdx);
      setFocusedIdx(-1);
      return true;
    }
    return false;
  };

  useEffect(() => {
    nameRef.current?.focus();
    nameRef.current?.select();
  }, []);

  const submit = (): void => {
    void validate(authoring.name, authoring.composedQuery, onSave);
  };

  // Abort in-flight probe so a late resolve can't invoke onSave after cancel.
  const cancel = (): void => {
    abort();
    onCancel();
  };

  return (
    <Modal onClose={cancel}>
      <div
        style={{
          width: "min(520px, 92vw)",
          height: "min(360px, 86vh)",
          padding: "16px 18px",
          backgroundImage:
            "linear-gradient(180deg, rgba(255, 255, 255, 0.025) 0%, rgba(255, 255, 255, 0) 60%)",
          backgroundColor: "var(--surface-modal)",
          backdropFilter: "var(--surface-blur)",
          WebkitBackdropFilter: "var(--surface-blur)",
          border: "1px solid var(--accent-border)",
          borderRadius: "var(--radius-md)",
          boxShadow:
            "inset 0 1px 0 var(--surface-glass-highlight), var(--surface-shadow)",
          display: "flex",
          flexDirection: "column",
          gap: 10,
        }}
        onClick={(e) => e.stopPropagation()}
      >
        <header
          style={{
            display: "flex",
            flexDirection: "column",
            gap: 6,
            paddingBottom: 8,
            borderBottom: "1px solid var(--border-subtle)",
            marginBottom: 2,
          }}
        >
          <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
            <span
              style={{
                fontFamily: "var(--font-ui)",
                fontSize: "var(--fs-xl)",
                fontWeight: 600,
                color: "var(--fg-base)",
                letterSpacing: "0.01em",
              }}
            >
              New query
            </span>
            <span
              style={{
                fontFamily: "var(--font-mono)",
                fontSize: "var(--fs-md)",
                color: "var(--accent-text-wash)",
                background: "var(--accent-wash)",
                border: "1px solid var(--accent-border)",
                borderRadius: "var(--radius-sm)",
                padding: "1px 8px",
              }}
              title="Session and bus this query will run on"
            >
              {sessionName}.{busName}
            </span>
          </div>
          <div style={{ display: "flex", alignItems: "center" }}>
            <ModeToggle value={authoring.mode} onChange={authoring.switchMode} />
          </div>
        </header>
        <FieldRow label="Name">
          <ThemedInput
            ref={nameRef}
            type="text"
            value={authoring.name}
            onChange={(e) => authoring.setName(e.target.value)}
            placeholder="query name"
            spellCheck={false}
            autoComplete="off"
            onKeyDown={(e) => {
              if (e.key === "Escape") cancel();
            }}
          />
        </FieldRow>
        <div
          style={{
            flex: 1,
            minHeight: 0,
            display: "flex",
            flexDirection: "column",
            gap: 8,
            overflow: "auto",
          }}
        >
        {authoring.mode === "guided" ? (
          <>
            <FieldRow label="SELECT">
              <span style={{ display: "flex", alignItems: "center", gap: 6, flex: 1 }}>
                <ThemedInput
                  type="text"
                  value={authoring.classSpec}
                  onChange={(e) => authoring.setClassSpec(e.target.value)}
                  placeholder="* or ClassName"
                  spellCheck={false}
                  autoComplete="off"
                  extraStyle={{ flex: 1 }}
                  onKeyDown={(e) => {
                    if (
                      navigateSuggestions(
                        e,
                        classFocus,
                        setClassFocus,
                        classMatchesInfo.matches.length,
                        (idx) => authoring.setClassSpec(classMatchesInfo.matches[idx]!),
                      )
                    ) {
                      return;
                    }
                    if ((e.metaKey || e.ctrlKey) && e.key === "Enter") {
                      e.preventDefault();
                      submit();
                    } else if (e.key === "Escape") {
                      cancel();
                    }
                  }}
                />
                <span
                  style={{
                    fontFamily: "var(--font-mono)",
                    fontSize: "var(--fs-sm)",
                    color: "var(--fg-subtle)",
                    whiteSpace: "nowrap",
                  }}
                  title="FROM clause is fixed by the bus you're authoring in"
                >
                  FROM {sessionName}.{busName}
                </span>
              </span>
            </FieldRow>
            <div style={{ paddingLeft: 56 }}>
              <ClassSuggestions
                matches={classMatchesInfo.matches}
                knownCount={classMatchesInfo.knownCount}
                focusedIdx={classFocus}
                onPick={authoring.setClassSpec}
              />
            </div>
            <FieldRow label="WHERE">
              <ThemedInput
                ref={whereRef}
                type="text"
                value={authoring.whereClause}
                onChange={(e) => {
                  authoring.setWhereClause(e.target.value);
                  completion.syncCaret();
                }}
                onSelect={completion.syncCaret}
                onClick={completion.syncCaret}
                onKeyUp={completion.syncCaret}
                onFocus={completion.syncCaret}
                placeholder="(optional)"
                spellCheck={false}
                autoComplete="off"
                onKeyDown={(e) => {
                  if (
                    navigateSuggestions(
                      e,
                      whereFocus,
                      setWhereFocus,
                      whereMatchesInfo.items.length,
                      (idx) => completion.replaceWordWithToken(whereMatchesInfo.items[idx]!.token),
                    )
                  ) {
                    return;
                  }
                  if ((e.metaKey || e.ctrlKey) && e.key === "Enter") {
                    e.preventDefault();
                    submit();
                  } else if (e.key === "Escape") {
                    cancel();
                  }
                }}
              />
            </FieldRow>
            <div style={{ paddingLeft: 56 }}>
              <WhereGuidance
                className={authoring.classSpec.trim() || "*"}
                properties={properties}
                currentWord={completion.currentWord.word}
                matches={whereMatchesInfo.items}
                rowOneCount={whereMatchesInfo.rowOneCount}
                truncated={whereMatchesInfo.truncated}
                focusedIdx={whereFocus}
                onPick={completion.replaceWordWithToken}
              />
            </div>
          </>
        ) : (
          <FieldRow label="SQL" align="stretch">
            <ThemedTextarea
              value={authoring.rawSql}
              onChange={(e) => authoring.setRawSql(e.target.value)}
              spellCheck={false}
              autoComplete="off"
              extraStyle={{ resize: "none", flex: 1, minHeight: 0 }}
              onKeyDown={(e) => {
                if ((e.metaKey || e.ctrlKey) && e.key === "Enter") {
                  e.preventDefault();
                  submit();
                } else if (e.key === "Escape") {
                  cancel();
                }
              }}
            />
          </FieldRow>
        )}
        </div>
        {error && (
          <div
            style={{
              fontSize: "var(--fs-sm)",
              color: "var(--err)",
              fontFamily: "var(--font-mono)",
              background: "var(--bg-elevated)",
              border: "1px solid var(--err)",
              borderRadius: "var(--radius-sm)",
              padding: "4px 6px",
            }}
          >
            {error}
          </div>
        )}
        <div style={{ display: "flex", gap: 6, alignItems: "center" }}>
          <button
            type="button"
            className="pressable pressable--accent"
            onClick={submit}
            disabled={submitting}
            style={{
              height: 28,
              padding: "0 12px",
              fontSize: "var(--fs-lg)",
              fontFamily: "var(--font-ui)",
            }}
          >
            {submitting ? "Validating..." : "Save"}
          </button>
          <button
            type="button"
            className="pressable"
            onClick={cancel}
            style={{
              height: 28,
              padding: "0 12px",
              fontSize: "var(--fs-lg)",
              fontFamily: "var(--font-ui)",
              color: "var(--fg-muted)",
            }}
          >
            Cancel
          </button>
          <span style={{ marginLeft: "auto", fontSize: "var(--fs-xs)", color: "var(--fg-subtle)" }}>⌘↵ to save</span>
        </div>
      </div>
    </Modal>
  );
}

function FieldRow({
  label,
  children,
  align = "center",
}: {
  label: string;
  children: ReactNode;
  align?: "center" | "stretch";
}) {
  return (
    <label
      style={{
        display: "flex",
        alignItems: align === "center" ? "center" : "stretch",
        gap: 8,
        flex: align === "stretch" ? 1 : "none",
        minHeight: 0,
      }}
    >
      <span
        style={{
          width: 52,
          fontSize: "var(--fs-xs)",
          textTransform: "uppercase",
          letterSpacing: "0.05em",
          color: "var(--fg-subtle)",
          flex: "none",
          paddingTop: align === "stretch" ? 6 : 0,
        }}
      >
        {label}
      </span>
      {children}
    </label>
  );
}
