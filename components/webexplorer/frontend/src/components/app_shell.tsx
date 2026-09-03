// === AppShell.tsx ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { memo, useCallback, useRef, useState } from "react";
import type * as React from "react";

// Three-pane shell: nav | detail | workspace. Left/right widths persist in localStorage.

const NAV_DEFAULT = 480;
const WS_DEFAULT = 480;
const PANE_MIN = 180;
const CENTER_MIN = 320;
// Sum of the nav-left and ws-right floating-card outer margins.
const PANE_OUTER_MARGINS = 24;
export const WS_STRIP_WIDTH = 24;
export const WS_FOLDED_WIDTH = WS_STRIP_WIDTH;

const NAV_KEY = "webex.navWidth";
const WS_KEY = "webex.wsWidth";

export interface AppShellProps {
  nav: React.ReactNode;
  detail: React.ReactNode;
  /** Workspace owns its own drag handle; no EdgeResizeHandle on the right. */
  workspace: (api: { onResize: (dx: number) => void; onResizeEnd: () => void }) => React.ReactNode;
  /** Folds the workspace to WS_FOLDED_WIDTH; stored wsWidth preserved. */
  wsHidden?: boolean;
  /** Flip in the same batch as `wsHidden` so the fold animates; off otherwise so splitter
   *  drags don't tween. */
  wsAnimating?: boolean;
}

function AppShellImpl({
  nav,
  detail,
  workspace,
  wsHidden = false,
  wsAnimating = false,
}: AppShellProps) {
  const [navWidth, setNavWidth] = useState(() => readStoredWidth(NAV_KEY, NAV_DEFAULT));
  const [wsWidth, setWsWidth] = useState(() => readStoredWidth(WS_KEY, WS_DEFAULT));

  // Persist on drag end so we don't setItem at the pointer rate.
  const commitNavWidth = useCallback(() => {
    try {
      localStorage.setItem(NAV_KEY, String(navWidth));
    } catch {
    }
  }, [navWidth]);
  const commitWsWidth = useCallback(() => {
    try {
      localStorage.setItem(WS_KEY, String(wsWidth));
    } catch {
    }
  }, [wsWidth]);

  const onDragNav = useCallback(
    (dx: number) => {
      setNavWidth((w) => {
        const maxNav = Math.max(PANE_MIN, window.innerWidth - wsWidth - CENTER_MIN - PANE_OUTER_MARGINS);
        return clamp(w + dx, PANE_MIN, maxNav);
      });
    },
    [wsWidth],
  );
  const onDragWs = useCallback(
    (dx: number) => {
      setWsWidth((w) => {
        const maxWs = Math.max(PANE_MIN, window.innerWidth - navWidth - CENTER_MIN - PANE_OUTER_MARGINS);
        return clamp(w - dx, PANE_MIN, maxWs);
      });
    },
    [navWidth],
  );

  const wsColPx = wsHidden ? WS_FOLDED_WIDTH : wsWidth;
  return (
    <div
      style={{
        display: "grid",
        gridTemplateRows: "1fr",
        gridTemplateColumns: `${navWidth}px 1fr ${wsColPx}px`,
        gridTemplateAreas: `"nav detail ws"`,
        height: "100%",
        transition: wsAnimating ? "grid-template-columns 220ms ease" : "none",
      }}
    >
      <aside
        style={{
          gridArea: "nav",
          position: "relative",
          margin:
            "var(--pane-gutter-top) var(--pane-gutter-side) var(--pane-gutter-top) var(--pane-gutter-side)",
          minWidth: 0,
          overflow: "hidden",
          paddingRight: EDGE_HANDLE_HIT,
        }}
      >
        {nav}
        <EdgeResizeHandle
          side="right"
          onDrag={onDragNav}
          onDragEnd={commitNavWidth}
          ariaLabel="resize navigation pane"
        />
      </aside>
      <main style={{ gridArea: "detail", overflow: "auto", minWidth: 0 }}>
        {detail}
      </main>
      <aside
        style={{
          gridArea: "ws",
          position: "relative",
          // Flush against the viewport's right edge; only the left edge rounds.
          margin: "var(--pane-gutter-top) 0 var(--pane-gutter-top) 0",
          borderRadius: "var(--radius-xl) 0 0 var(--radius-xl)",
          border: "1px solid var(--border-glass)",
          borderRight: "none",
          boxShadow: "var(--surface-shadow), var(--surface-glass-bevel)",
          overflow: "hidden",
          background: "var(--surface-glass-top-sheen), var(--surface-pane)",
          backdropFilter: "var(--surface-blur)",
          WebkitBackdropFilter: "var(--surface-blur)",
          minWidth: 0,
        }}
      >
        <div
          style={{
            position: "absolute",
            inset: 0,
            overflow: "hidden",
          }}
        >
          {workspace({ onResize: onDragWs, onResizeEnd: commitWsWidth })}
        </div>
      </aside>
    </div>
  );
}

const EDGE_HANDLE_HIT = 6;

function EdgeResizeHandle({
  side,
  onDrag,
  onDragEnd,
  ariaLabel,
}: {
  side: "left" | "right";
  onDrag: (dx: number) => void;
  onDragEnd?: () => void;
  ariaLabel: string;
}) {
  const [hover, setHover] = useState(false);
  const [active, setActive] = useState(false);
  const lastXRef = useRef(0);
  const draggedRef = useRef(false);

  const onPointerDown = (e: React.PointerEvent<HTMLDivElement>) => {
    e.preventDefault();
    lastXRef.current = e.clientX;
    draggedRef.current = false;
    setActive(true);
    e.currentTarget.setPointerCapture(e.pointerId);
  };
  const onKeyDown = (e: React.KeyboardEvent<HTMLDivElement>) => {
    const step = e.shiftKey ? 40 : 10;
    let dx = 0;
    if (e.key === "ArrowLeft") dx = -step;
    else if (e.key === "ArrowRight") dx = step;
    else if (e.key === "Home") dx = -10000;
    else if (e.key === "End") dx = 10000;
    else return;
    e.preventDefault();
    onDrag(dx);
    onDragEnd?.();
  };
  const onPointerMove = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!active) return;
    const dx = e.clientX - lastXRef.current;
    if (dx === 0) return;
    lastXRef.current = e.clientX;
    draggedRef.current = true;
    onDrag(dx);
  };
  const stop = (e: React.PointerEvent<HTMLDivElement>) => {
    if (!active) return;
    setActive(false);
    if (draggedRef.current && onDragEnd) onDragEnd();
    draggedRef.current = false;
    try {
      e.currentTarget.releasePointerCapture(e.pointerId);
    } catch {
    }
  };

  const isActive = hover || active;
  const positionStyle =
    side === "right"
      ? { right: 0 }
      : { left: 0 };
  return (
    <div
      role="separator"
      aria-orientation="vertical"
      aria-label={ariaLabel}
      tabIndex={0}
      onKeyDown={onKeyDown}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={stop}
      onPointerCancel={stop}
      onPointerEnter={() => setHover(true)}
      onPointerLeave={() => setHover(false)}
      style={{
        position: "absolute",
        ...positionStyle,
        top: 0,
        bottom: 0,
        width: EDGE_HANDLE_HIT,
        cursor: "col-resize",
        userSelect: "none",
        touchAction: "none",
        zIndex: 2,
      }}
    >
      <div
        style={{
          position: "absolute",
          [side]: 0,
          top: 0,
          bottom: 0,
          width: isActive ? 5 : 4,
          background: isActive ? "var(--accent)" : "var(--border-default)",
          opacity: active ? 0.8 : hover ? 0.65 : 0.45,
          transition: active
            ? "none"
            : "background 0.12s ease, width 0.12s ease, opacity 0.12s ease",
        }}
      />
    </div>
  );
}

export const AppShell = memo(AppShellImpl);

function readStoredWidth(key: string, fallback: number): number {
  try {
    const raw = localStorage.getItem(key);
    if (!raw) return fallback;
    const n = parseInt(raw, 10);
    return Number.isFinite(n) && n >= PANE_MIN ? n : fallback;
  } catch {
    return fallback;
  }
}

function clamp(v: number, lo: number, hi: number): number {
  return Math.max(lo, Math.min(hi, v));
}
