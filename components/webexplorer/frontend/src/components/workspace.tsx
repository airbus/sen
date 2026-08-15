// === Workspace.tsx ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { memo, useRef } from "react";
import type * as React from "react";

import type { Client } from "@sen/client";

import { GripIcon, WatchIcon } from "../ui/icons.js";
import { WS_STRIP_WIDTH } from "./app_shell.js";
import { WatchTab } from "./watch_tab.js";

export interface WorkspaceProps {
  client: Client | null;
  folded: boolean;
  onToggleFolded: () => void;
  /** Horizontal pixel deltas during a drag; only meaningful when unfolded. */
  onResize: (dx: number) => void;
  /** Fired once at pointer-up when the drag actually moved the pane. */
  onResizeEnd: () => void;
}

// memo'd: parent rebuilds the props on every drag tick; shallow compare avoids rebuilding
// the WatchTab subtree on every parent render.
export const Workspace = memo(function Workspace({ client, folded, onToggleFolded, onResize, onResizeEnd }: WorkspaceProps) {
  if (folded) {
    return (
      <SideStrip
        ariaLabel="show watch pane"
        title="Show Watch pane"
        onClick={onToggleFolded}
      />
    );
  }
  return (
    <div style={{ display: "flex", flexDirection: "row", height: "100%" }}>
      <SideStrip
        ariaLabel="collapse watch pane (drag to resize)"
        title="Collapse Watch pane · drag to resize"
        onClick={onToggleFolded}
        onResize={onResize}
        onResizeEnd={onResizeEnd}
      />
      <div style={{ flex: 1, minHeight: 0, minWidth: 0, overflow: "hidden", display: "flex", flexDirection: "column" }}>
        <WatchTab client={client} />
      </div>
    </div>
  );
});

// Pointer travel >= threshold commits to a drag; pointer-up then skips the click toggle.
const DRAG_THRESHOLD_PX = 4;

function SideStrip({
  onClick,
  onResize,
  onResizeEnd,
  ariaLabel,
  title,
}: {
  onClick: () => void;
  onResize?: (dx: number) => void;
  onResizeEnd?: () => void;
  ariaLabel: string;
  title: string;
}) {
  const dragRef = useRef({ active: false, startX: 0, lastX: 0, dragged: false });

  const handlePointerDown = (e: React.PointerEvent<HTMLButtonElement>) => {
    if (!onResize) return;
    dragRef.current = { active: true, startX: e.clientX, lastX: e.clientX, dragged: false };
    e.currentTarget.setPointerCapture(e.pointerId);
  };
  const handlePointerMove = (e: React.PointerEvent<HTMLButtonElement>) => {
    if (!onResize) return;
    const d = dragRef.current;
    if (!d.active) return;
    if (!d.dragged && Math.abs(e.clientX - d.startX) < DRAG_THRESHOLD_PX) return;
    d.dragged = true;
    const dx = e.clientX - d.lastX;
    if (dx === 0) return;
    d.lastX = e.clientX;
    onResize(dx);
  };
  const handlePointerUp = (e: React.PointerEvent<HTMLButtonElement>) => {
    if (!onResize) {
      onClick();
      return;
    }
    const d = dragRef.current;
    d.active = false;
    try {
      e.currentTarget.releasePointerCapture(e.pointerId);
    } catch {
    }
    if (d.dragged) {
      onResizeEnd?.();
    } else {
      onClick();
    }
  };
  const handlePointerCancel = (e: React.PointerEvent<HTMLButtonElement>) => {
    dragRef.current.active = false;
    try {
      e.currentTarget.releasePointerCapture(e.pointerId);
    } catch {
    }
  };
  // No onClick (pointer-up dispatches so we can suppress on drag); wire keys for a11y.
  const handleKeyDown = (e: React.KeyboardEvent<HTMLButtonElement>) => {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      onClick();
    }
  };

  return (
    <button
      type="button"
      title={title}
      aria-label={ariaLabel}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerCancel}
      onKeyDown={handleKeyDown}
      style={{
        position: "relative",
        width: WS_STRIP_WIDTH,
        flex: "none",
        height: "100%",
        display: "flex",
        flexDirection: "column",
        alignItems: "center",
        gap: 6,
        padding: "8px 0",
        border: "none",
        borderRight: onResize ? "1px solid var(--border-default)" : "none",
        background: "var(--surface-header)",
        color: "var(--fg-base)",
        cursor: onResize ? "col-resize" : "pointer",
        userSelect: "none",
        touchAction: "none",
        fontFamily: "var(--font-ui)",
      }}
    >
      {/* Grip floats at the strip's vertical middle as the drag affordance; absolute so
          the icon+label flow stays anchored to the top. */}
      <span
        aria-hidden="true"
        style={{
          position: "absolute",
          left: "50%",
          top: "50%",
          transform: "translate(-50%, -50%)",
          color: "var(--fg-subtle)",
          opacity: 0.5,
          pointerEvents: "none",
          display: "inline-flex",
        }}
      >
        <GripIcon />
      </span>
      <span style={{ display: "inline-flex", color: "var(--fg-muted)" }}>
        <WatchIcon />
      </span>
      <span
        style={{
          writingMode: "vertical-rl",
          transform: "rotate(180deg)",
          fontSize: "var(--fs-sm)",
          fontWeight: 600,
          letterSpacing: "0.12em",
          textTransform: "uppercase",
          color: "var(--fg-muted)",
        }}
      >
        Watch
      </span>
    </button>
  );
}
