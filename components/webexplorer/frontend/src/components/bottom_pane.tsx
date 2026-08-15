// === BottomPane.tsx ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { createContext, useRef, useState, type ReactNode, type RefObject } from "react";
import type * as React from "react";

import { GripIcon } from "../ui/icons.js";

// Context-provided portal target for the active tab's header controls.
export const BottomPaneHeaderSlot = createContext<HTMLDivElement | null>(null);

const HEADER_HEIGHT = 30;
const MIN_BODY = 80;
const MAX_FRACTION = 0.8;
const DRAG_THRESHOLD_PX = 4;

export interface BottomPaneTab {
  key: string;
  title: string;
  icon?: ReactNode;
  count?: number;
  /** Inline control right of the tab label; clicks here don't switch tabs. */
  trailing?: ReactNode;
  /** Inactive tabs are unmounted; per-tab state must live in module stores or self-persist. */
  content: ReactNode;
}

export interface BottomPaneProps {
  tabs: readonly BottomPaneTab[];
  activeTabKey: string;
  onActiveTabChange: (key: string) => void;
  folded: boolean;
  onToggleFolded: () => void;
  height: number;
  onResize: (height: number) => void;
  /** Final-height callback at drag end; persistence runs here, not per pointer event. */
  onResizeCommit: (height: number) => void;
  /** Self-margins (not wrapper paddings) so the resize geometry walk still lands on the
   *  enclosing flex column rather than a wrapper div. */
  marginLeft?: number;
  marginRight?: number;
}

export function BottomPane({
  tabs,
  activeTabKey,
  onActiveTabChange,
  folded,
  onToggleFolded,
  height,
  onResize,
  onResizeCommit,
  marginLeft = 0,
  marginRight = 0,
}: BottomPaneProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const [dragging, setDragging] = useState(false);
  const [headerSlot, setHeaderSlot] = useState<HTMLDivElement | null>(null);

  // Fall back to the first tab if the persisted key no longer matches.
  const activeTab = tabs.find((t) => t.key === activeTabKey) ?? tabs[0]!;

  const effectiveHeight = folded ? HEADER_HEIGHT : height;

  const onTabClick = (key: string) => {
    if (folded) onToggleFolded();
    if (key !== activeTab.key) onActiveTabChange(key);
  };

  return (
    <div
      ref={containerRef}
      style={{
        height: effectiveHeight,
        display: "flex",
        flexDirection: "column",
        borderTop: "1px solid var(--border-glass)",
        borderRadius: "var(--radius-xl) var(--radius-xl) 0 0",
        background: "var(--surface-glass-top-sheen), var(--surface-pane)",
        backdropFilter: "var(--surface-blur)",
        WebkitBackdropFilter: "var(--surface-blur)",
        flex: "none",
        marginLeft,
        marginRight,
        transition: dragging ? "none" : "height 200ms ease",
        overflow: "hidden",
        boxShadow: "var(--surface-glass-bevel)",
      }}
    >
      <Header
        tabs={tabs}
        activeTabKey={activeTab.key}
        onTabClick={onTabClick}
        folded={folded}
        onToggleFolded={onToggleFolded}
        onResize={onResize}
        onResizeCommit={onResizeCommit}
        containerRef={containerRef}
        setDragging={setDragging}
        slotRef={folded ? null : setHeaderSlot}
      />
      {!folded && (
        <BottomPaneHeaderSlot.Provider value={headerSlot}>
          <div
            key={activeTab.key}
            style={{ flex: 1, minHeight: 0, overflow: "hidden", display: "flex", flexDirection: "column" }}
          >
            {activeTab.content}
          </div>
        </BottomPaneHeaderSlot.Provider>
      )}
    </div>
  );
}

function Header({
  tabs,
  activeTabKey,
  onTabClick,
  folded,
  onToggleFolded,
  onResize,
  onResizeCommit,
  containerRef,
  setDragging,
  slotRef,
}: {
  tabs: readonly BottomPaneTab[];
  activeTabKey: string;
  onTabClick: (key: string) => void;
  folded: boolean;
  onToggleFolded: () => void;
  onResize: (height: number) => void;
  onResizeCommit: (height: number) => void;
  containerRef: RefObject<HTMLDivElement | null>;
  setDragging: (dragging: boolean) => void;
  slotRef: ((node: HTMLDivElement | null) => void) | null;
}) {
  // Pointer-capture drag + click fallback; tab buttons stopPropagation so this only fires
  // on the empty header area.
  const dragRef = useRef({ active: false, startY: 0, dragged: false });
  const lastHeightRef = useRef(0);

  const computeAndApplyHeight = (clientY: number) => {
    const parent = containerRef.current?.parentElement;
    if (!parent) return;
    const parentRect = parent.getBoundingClientRect();
    const raw = parentRect.bottom - clientY;
    const min = HEADER_HEIGHT + MIN_BODY;
    const max = Math.max(min, parentRect.height * MAX_FRACTION);
    const next = Math.max(min, Math.min(max, raw));
    lastHeightRef.current = next;
    onResize(next);
  };

  const handlePointerDown = (e: React.PointerEvent<HTMLDivElement>) => {
    e.preventDefault();
    if (folded) {
      dragRef.current = { active: true, startY: e.clientY, dragged: false };
      return;
    }
    if (e.button !== 0) return;
    dragRef.current = { active: true, startY: e.clientY, dragged: false };
    e.currentTarget.setPointerCapture(e.pointerId);
  };
  const handlePointerMove = (e: React.PointerEvent<HTMLDivElement>) => {
    const d = dragRef.current;
    if (!d.active || folded) return;
    if (!d.dragged && Math.abs(e.clientY - d.startY) < DRAG_THRESHOLD_PX) return;
    if (!d.dragged) {
      d.dragged = true;
      setDragging(true);
    }
    computeAndApplyHeight(e.clientY);
  };
  const handlePointerUp = (e: React.PointerEvent<HTMLDivElement>) => {
    const d = dragRef.current;
    const wasDrag = d.dragged;
    d.active = false;
    if (!folded) {
      try {
        e.currentTarget.releasePointerCapture(e.pointerId);
      } catch {
      }
    }
    if (wasDrag) {
      setDragging(false);
      onResizeCommit(lastHeightRef.current);
    } else {
      onToggleFolded();
    }
  };
  const handlePointerCancel = (e: React.PointerEvent<HTMLDivElement>) => {
    const d = dragRef.current;
    d.active = false;
    if (!folded) {
      try {
        e.currentTarget.releasePointerCapture(e.pointerId);
      } catch {
      }
    }
    if (d.dragged) setDragging(false);
  };
  const handleKeyDown = (e: React.KeyboardEvent<HTMLDivElement>) => {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      onToggleFolded();
    }
  };

  return (
    <div
      role="button"
      tabIndex={0}
      aria-expanded={!folded}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerCancel}
      onKeyDown={handleKeyDown}
      style={{
        position: "relative",
        display: "flex",
        alignItems: "stretch",
        gap: 4,
        padding: "0 6px",
        height: HEADER_HEIGHT,
        background: "var(--surface-header)",
        borderBottom: folded ? "none" : "1px solid var(--border-subtle)",
        cursor: folded ? "pointer" : "row-resize",
        userSelect: "none",
        touchAction: "none",
        flex: "none",
      }}
    >
      {folded && (
        <span
          aria-hidden="true"
          style={{
            position: "absolute",
            left: "50%",
            top: "50%",
            transform: "translate(-50%, -50%) rotate(90deg)",
            color: "var(--fg-subtle)",
            opacity: 0.5,
            pointerEvents: "none",
            display: "inline-flex",
          }}
        >
          <GripIcon />
        </span>
      )}
      <div
        onPointerDown={(e) => e.stopPropagation()}
        style={{
          display: "inline-flex",
          alignItems: "stretch",
          gap: 4,
          alignSelf: "stretch",
        }}
      >
        {tabs.map((t) => (
          <TabButton
            key={t.key}
            tab={t}
            active={t.key === activeTabKey && !folded}
            onClick={() => onTabClick(t.key)}
          />
        ))}
      </div>
      <span
        style={{
          marginLeft: "auto",
          display: "inline-flex",
          alignItems: "center",
          gap: 10,
          color: "var(--fg-muted)",
        }}
      >
        {/* Pointer events stopped so child controls (slider scrub) don't trip fold/resize. */}
        {slotRef && (
          <span
            ref={slotRef}
            onClick={(e) => e.stopPropagation()}
            onPointerDown={(e) => e.stopPropagation()}
            onPointerMove={(e) => e.stopPropagation()}
            onPointerUp={(e) => e.stopPropagation()}
            style={{ display: "inline-flex", alignItems: "center", gap: 8 }}
          />
        )}
      </span>
    </div>
  );
}

function TabButton({
  tab,
  active,
  onClick,
}: {
  tab: BottomPaneTab;
  active: boolean;
  onClick: () => void;
}) {
  const [hover, setHover] = useState(false);
  const fg = active
    ? "var(--fg-base)"
    : hover
      ? "var(--accent-text-wash)"
      : "var(--fg-muted)";
  // stopPropagation so the header's drag-or-fold handler doesn't swallow tab clicks; we
  // dispatch onClick from pointerdown directly so pure-click sequences still activate.
  const stop = (e: React.SyntheticEvent) => e.stopPropagation();
  return (
    <span
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        display: "inline-flex",
        alignItems: "stretch",
        boxShadow: active ? "inset 0 -2px 0 0 var(--accent)" : "none",
        transition: "box-shadow 120ms ease",
      }}
    >
      <button
        type="button"
        onPointerDown={(e) => {
          e.stopPropagation();
          onClick();
        }}
        onPointerMove={stop}
        onPointerUp={stop}
        onClick={stop}
        title={tab.title}
        aria-pressed={active}
        style={{
          display: "inline-flex",
          alignItems: "center",
          gap: 6,
          padding: "0 8px",
          border: "none",
          background: "transparent",
          color: fg,
          fontFamily: "var(--font-ui)",
          fontSize: "var(--fs-lg)",
          fontWeight: active ? 500 : 400,
          cursor: "pointer",
          borderRadius: 0,
          transition: "color 120ms ease",
        }}
      >
        {tab.icon && (
          <span style={{ display: "inline-flex", color: active ? "var(--fg-muted)" : "var(--fg-subtle)" }}>
            {tab.icon}
          </span>
        )}
        {tab.title}
        {tab.count !== undefined && tab.count > 0 && (
          <span
            style={{
              fontSize: "var(--fs-sm)",
              color: "var(--fg-subtle)",
              fontFamily: "var(--font-mono)",
            }}
          >
            {tab.count}
          </span>
        )}
      </button>
      {tab.trailing && (
        <span
          onPointerDown={stop}
          onPointerMove={stop}
          onPointerUp={stop}
          onClick={stop}
          style={{
            display: "inline-flex",
            alignItems: "center",
            paddingRight: 6,
          }}
        >
          {tab.trailing}
        </span>
      )}
    </span>
  );
}
