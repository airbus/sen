// === settings_popover.tsx ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";

import { applySnapshot, captureSnapshot, type LayoutSnapshot } from "../state/layout_snapshot.js";
import { layoutsActions, useLayouts, type NamedLayout } from "../state/layouts_store.js";
import { RETENTION_BOUNDS, settingsActions, useSettings } from "../state/settings.js";
import { recentUrlsActions, useRecentUrls } from "../state/ui_prefs.js";
import { HoverIconButton } from "../ui/buttons.js";
import { GearIcon } from "../ui/icons.js";

export function SettingsPopover({
  connectedUrl,
  onChangeUrl,
  connectionState,
  showSuccessMsg
}: {
  connectedUrl: string;
  onChangeUrl: (next: string) => void;
  connectionState: string;
  showSuccessMsg: boolean;
}) {
  const settings = useSettings();
  const update = settingsActions.update;
  const [open, setOpen] = useState(false);
  const [closing, setClosing] = useState(false);
  const [draftUrl, setDraftUrl] = useState("");
  const anchorRef = useRef<HTMLSpanElement | null>(null);
  // Viewport coords so the portal escapes any clipping ancestor's `overflow: hidden`.
  const [coords, setCoords] = useState<{ top: number; right: number } | null>(null);
  const recentUrls = useRecentUrls();

  const closeSmoothly = useCallback(() => {
    setClosing(true);
    setTimeout(() => {
      setOpen(false);
      setClosing(false);
    }, 150);
  }, []);

  const recompute = () => {
    const r = anchorRef.current?.getBoundingClientRect();
    if (!r) return;
    setCoords({ top: r.bottom + 6, right: window.innerWidth - r.right });
  };
  useLayoutEffect(() => {
    if (!open) return;
    recompute();
    window.addEventListener("resize", recompute);
    window.addEventListener("scroll", recompute, true);
    return () => {
      window.removeEventListener("resize", recompute);
      window.removeEventListener("scroll", recompute, true);
    };
  }, [open]);

  useEffect(() => {
    if (open) {
      setDraftUrl("");
    }
  }, [open]);

  const prevConnectedUrl = useRef(connectedUrl);
  useEffect(() => {
    if (open && prevConnectedUrl.current !== connectedUrl) {
      closeSmoothly();
      setDraftUrl("");
    }
    prevConnectedUrl.current = connectedUrl;
  }, [connectedUrl, open, closeSmoothly]);

  useEffect(() => {
    if (!open) return;
    const onDocClick = (e: MouseEvent) => {
      const target = e.target as Node | null;
      if (target && anchorRef.current?.contains(target)) return;
      const popoverEl = document.getElementById("webex-settings-popover");
      if (target && popoverEl?.contains(target)) return;
      closeSmoothly();
    };
    const onEsc = (e: KeyboardEvent) => {
      if (e.key === "Escape") closeSmoothly();
    };
    document.addEventListener("mousedown", onDocClick);
    document.addEventListener("keydown", onEsc);
    return () => {
      document.removeEventListener("mousedown", onDocClick);
      document.removeEventListener("keydown", onEsc);
    };
  }, [open, closeSmoothly]);

  const toggleOpen = () => {
    if (open) {
      closeSmoothly();
    } else {
      setOpen(true);
    }
  };

  return (
    <span ref={anchorRef} style={{ display: "inline-flex" }}>
      <HoverIconButton
        onClick={toggleOpen}
        ariaLabel={open ? "close settings" : "open settings"}
        tooltip="Settings"
        icon={<GearIcon />}
      />
      {open && coords && createPortal(
        <div
          id="webex-settings-popover"
          role="dialog"
          aria-label="Explorer settings"
          style={{
            position: "fixed",
            top: coords.top,
            right: coords.right,
            width: 260,
            padding: "10px 12px 12px",
            background: "var(--bg-elevated)",
            border: "1px solid var(--border-strong)",
            borderRadius: "var(--radius-lg)",
            boxShadow: "0 12px 28px rgba(0, 0, 0, 0.45)",
            zIndex: 10000,
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            color: "var(--fg-base)",
            display: "flex",
            flexDirection: "column",
            gap: 10,
            opacity: closing ? 0 : 1,
            transform: closing ? "scale(0.98)" : "scale(1)",
            transition: "opacity 150ms ease, transform 150ms ease",
            transformOrigin: "top right",
          }}
        >
          <div
            style={{
              fontSize: "var(--fs-xs)",
              textTransform: "uppercase",
              letterSpacing: "0.05em",
              color: "var(--fg-subtle)",
            }}
          >
            Connection
          </div>
          <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
            <span style={{ color: "var(--fg-base)", fontSize: "var(--fs-sm)" }}>
              Backend Connection URL
            </span>
            <div style={{ display: "flex", gap: 4 }}>
              <input
                type="text"
                value={draftUrl}
                placeholder={connectedUrl || "ws://127.0.0.1:8080"}
                onChange={(e) => setDraftUrl(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === "Enter") {
                    const next = draftUrl.trim();
                    if (next && next !== connectedUrl) {
                      onChangeUrl(next);
                    }
                  }
                }}
                style={{
                  flex: 1,
                  background: "var(--bg-input)",
                  border: "1px solid var(--border-default)",
                  borderRadius: "var(--radius-sm)",
                  padding: "2px 6px",
                  color: "var(--fg-base)",
                  fontFamily: "var(--font-mono)",
                  fontSize: "var(--fs-sm)",
                  outline: "none",
                  minWidth: 0,
                }}
              />
              <LayoutBtn
                label="connect"
                disabled={draftUrl.trim() === connectedUrl || draftUrl.trim() === ""}
                onClick={() => {
                  const next = draftUrl.trim();
                  if (next && next !== connectedUrl) {
                    onChangeUrl(next);
                  }
                }} />
            </div>
            {recentUrls.length > 0 && (
              <div style={{ display: "flex", flexDirection: "column", gap: 2, marginTop: 4, maxHeight: 120, overflowY: "auto" }}>
                {recentUrls.map((rUrl) => (
                  <div
                    key={rUrl}
                    style={{
                      display: "flex",
                      alignItems: "center",
                      gap: 4,
                      padding: "2px 4px",
                      borderRadius: "var(--radius-sm)",
                    }}
                  >
                    <span
                      style={{
                        flex: 1,
                        minWidth: 0,
                        fontSize: "var(--fs-md)",
                        whiteSpace: "nowrap",
                        overflow: "hidden",
                        textOverflow: "ellipsis",
                        cursor: rUrl === connectedUrl ? "default" : "pointer",
                        color: rUrl === connectedUrl ? "var(--ok)" : "var(--fg-base)",
                      }}
                      onClick={() => {
                        if (rUrl !== connectedUrl) {
                          onChangeUrl(rUrl);
                        }
                      }}
                      title={rUrl}
                    >
                      {rUrl}
                    </span>
                    <LayoutBtn
                      label="✕"
                      tone="danger"
                      title="Remove URL"
                      onClick={() => recentUrlsActions.remove(rUrl)}
                    />
                  </div>
                ))}
              </div>
            )}
          </div>
          <div
            style={{
              fontSize: "var(--fs-xs)",
              textTransform: "uppercase",
              letterSpacing: "0.05em",
              color: "var(--fg-subtle)",
              borderTop: "1px solid var(--border-default)",
              paddingTop: 8,
            }}
          >
            Retention
          </div>
          <RetentionField
            label="Samples"
            help="plots"
            value={settings.sampleRetentionSeconds}
            onChange={(v) => update({ sampleRetentionSeconds: v })}
          />
          <RetentionField
            label="Events"
            help="events log"
            value={settings.eventRetentionSeconds}
            onChange={(v) => update({ eventRetentionSeconds: v })}
          />
          <div
            style={{
              fontSize: "var(--fs-xs)",
              color: "var(--fg-subtle)",
              lineHeight: 1.4,
              borderTop: "1px solid var(--border-default)",
              paddingTop: 8,
            }}
          >
            Lower values reduce memory; higher values keep more history. Range{" "}
            {RETENTION_BOUNDS.min}-{RETENTION_BOUNDS.max} s.
          </div>
          <LayoutsSection />
        </div>,
        document.body,
      )}
    </span>
  );
}

// Apply replaces every `webex.*` key and reloads; imports accept the same shape captureSnapshot
// produces, bad input dropped silently.
function LayoutsSection() {
  const layouts = useLayouts();
  const [draftName, setDraftName] = useState("");
  const importInputRef = useRef<HTMLInputElement | null>(null);

  const sortedLayouts = useMemo(
    () => Array.from(layouts.values()).sort((a, b) => a.name.localeCompare(b.name)),
    [layouts],
  );

  const saveCurrent = () => {
    const name = draftName.trim();
    if (!name) return;
    layoutsActions.saveCurrentAs(name);
    setDraftName("");
  };

  const exportLayout = (layout: NamedLayout) => {
    const blob = new Blob([JSON.stringify(layout, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${sanitizeFilename(layout.name)}.layout.json`;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  };

  const importLayout = (file: File) => {
    void (async () => {
      try {
        const text = await file.text();
        const parsed = JSON.parse(text) as unknown;
        const layout = coerceLayout(parsed);
        if (!layout) return;
        layoutsActions.upsert(layout);
      } catch {
        /* ignore malformed input */
      }
    })();
  };

  return (
    <div style={{ borderTop: "1px solid var(--border-default)", paddingTop: 8, display: "flex", flexDirection: "column", gap: 8 }}>
      <div
        style={{
          fontSize: "var(--fs-xs)",
          textTransform: "uppercase",
          letterSpacing: "0.05em",
          color: "var(--fg-subtle)",
        }}
      >
        Layouts
      </div>
      {sortedLayouts.length === 0 ? (
        <div style={{ fontSize: "var(--fs-xs)", color: "var(--fg-subtle)", lineHeight: 1.4 }}>
          No saved layouts. Save the current cockpit below.
        </div>
      ) : (
        <div style={{ display: "flex", flexDirection: "column", gap: 4, maxHeight: 220, overflowY: "auto" }}>
          {sortedLayouts.map((layout) => (
            <div
              key={layout.name}
              style={{
                display: "flex",
                alignItems: "center",
                gap: 4,
                padding: "2px 4px",
                borderRadius: "var(--radius-sm)",
              }}
            >
              <span
                style={{
                  flex: 1,
                  minWidth: 0,
                  fontSize: "var(--fs-md)",
                  whiteSpace: "nowrap",
                  overflow: "hidden",
                  textOverflow: "ellipsis",
                }}
                title={`Saved ${new Date(layout.savedAt).toLocaleString()}`}
              >
                {layout.name}
              </span>
              <LayoutBtn
                label="apply"
                onClick={() => {
                  // applySnapshot triggers a hard reload; popouts close, in-flight invocations lost.
                  if (
                    confirm(
                      `Apply layout "${layout.name}"? The page will reload; popout windows close and any in-flight method invocations are lost.`,
                    )
                  ) {
                    applySnapshot(layout.snapshot);
                  }
                }}
              />
              <LayoutBtn label="export" onClick={() => exportLayout(layout)} />
              <LayoutBtn
                label="✕"
                tone="danger"
                title="Delete layout"
                onClick={() => {
                  if (confirm(`Delete layout "${layout.name}"?`)) {
                    layoutsActions.remove(layout.name);
                  }
                }}
              />
            </div>
          ))}
        </div>
      )}
      <div style={{ display: "flex", gap: 4, alignItems: "center" }}>
        <input
          type="text"
          value={draftName}
          onChange={(e) => setDraftName(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter") saveCurrent();
          }}
          placeholder="Save current as..."
          style={{
            flex: 1,
            minWidth: 0,
            background: "var(--bg-input)",
            border: "1px solid var(--border-default)",
            borderRadius: "var(--radius-sm)",
            padding: "2px 6px",
            color: "var(--fg-base)",
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
          }}
        />
        <LayoutBtn label="save" onClick={saveCurrent} />
      </div>
      <div style={{ display: "flex", gap: 4 }}>
        <LayoutBtn label="import" onClick={() => importInputRef.current?.click()} />
        <input
          ref={importInputRef}
          type="file"
          accept="application/json,.json"
          style={{ display: "none" }}
          onChange={(e) => {
            const file = e.target.files?.[0];
            if (file) importLayout(file);
            e.target.value = "";
          }}
        />
      </div>
    </div>
  );
}

function LayoutBtn({
  label,
  onClick,
  tone,
  title,
  disabled
}: {
  label: string;
  onClick: () => void;
  tone?: "danger";
  title?: string;
  disabled?: boolean;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      title={title ?? label}
      disabled={disabled}
      style={{
        padding: "1px 6px",
        fontSize: "var(--fs-xs)",
        fontFamily: "var(--font-mono)",
        background: "transparent",
        color: tone === "danger" ? "var(--err)" : "var(--fg-subtle)",
        border: "1px solid var(--border-default)",
        borderRadius: "var(--radius-sm)",
        cursor: disabled ? "not-allowed" : "pointer",
        opacity: disabled ? 0.5 : 1,
      }}
    >
      {label}
    </button>
  );
}

function sanitizeFilename(name: string): string {
  return name.replace(/[^a-z0-9_.-]/gi, "_").slice(0, 80) || "layout";
}

function coerceLayout(raw: unknown): NamedLayout | null {
  if (!raw || typeof raw !== "object") return null;
  const r = raw as Partial<NamedLayout>;
  if (typeof r.name !== "string" || !r.name) return null;
  if (typeof r.savedAt !== "number") return null;
  if (!r.snapshot || typeof r.snapshot !== "object") return null;
  const snap: Record<string, string> = {};
  for (const [k, v] of Object.entries(r.snapshot as object)) {
    if (typeof k === "string" && typeof v === "string") snap[k] = v;
  }
  return { name: r.name, savedAt: r.savedAt, snapshot: snap as LayoutSnapshot };
}

function RetentionField({
  label,
  help,
  value,
  onChange,
}: {
  label: string;
  help: string;
  value: number;
  onChange: (next: number) => void;
}) {
  // Clamp + integer-coerce so a half-typed entry doesn't propagate as NaN.
  const commit = (raw: number) => {
    if (!Number.isFinite(raw)) return;
    const clamped = Math.max(
      RETENTION_BOUNDS.min,
      Math.min(RETENTION_BOUNDS.max, Math.round(raw)),
    );
    if (clamped !== value) onChange(clamped);
  };
  return (
    <label style={{ display: "flex", flexDirection: "column", gap: 4 }}>
      <span style={{ display: "flex", alignItems: "baseline", gap: 6 }}>
        <span style={{ color: "var(--fg-base)" }}>{label}</span>
        <span style={{ color: "var(--fg-subtle)", fontSize: "var(--fs-xs)" }}>{help}</span>
      </span>
      <span style={{ display: "flex", alignItems: "center", gap: 8 }}>
        <input
          type="range"
          min={RETENTION_BOUNDS.min}
          max={RETENTION_BOUNDS.max}
          value={value}
          onChange={(e) => commit(Number(e.target.value))}
          style={{ flex: 1, accentColor: "var(--accent)" }}
        />
        <input
          type="number"
          min={RETENTION_BOUNDS.min}
          max={RETENTION_BOUNDS.max}
          value={value}
          onChange={(e) => commit(Number(e.target.value))}
          style={{
            width: 56,
            background: "var(--bg-input)",
            border: "1px solid var(--border-default)",
            borderRadius: "var(--radius-sm)",
            padding: "2px 6px",
            color: "var(--fg-base)",
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
          }}
        />
        <span style={{ color: "var(--fg-subtle)", fontSize: "var(--fs-sm)" }}>s</span>
      </span>
    </label>
  );
}
