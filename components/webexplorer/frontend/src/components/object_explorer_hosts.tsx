// === ObjectExplorerHosts.tsx =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useLayoutEffect, useRef, useState } from "react";
import type * as React from "react";

import type { Client } from "@sen/client";

import type { Selection } from "../state/selection.js";
import type { PinnedObject } from "../state/ui_prefs.js";
import { BusChip, TypeChip } from "../ui/chips.js";
import { classSwatch } from "../ui/class_color.js";
import { CurrentExplorerProvider } from "../ui/explorer_links.js";
import { HoverIconButton } from "../ui/buttons.js";
import { BackIcon, PopOutIcon } from "../ui/icons.js";
import { Tooltip } from "../ui/tooltip.js";
import { DetailPane } from "./detail_pane.js";
import { InterestsPane } from "../widgets/interests/interests_pane.js";

// Backend interests live in InterestOwner at App root, so InterestsPane mount/unmount
// here doesn't affect wire state.
export function NavSwitcher({
  showExplorer,
  explorerSelection,
  onExplorerBack,
  onExplorerPopOut,
  client,
  pinnedObjects,
}: {
  showExplorer: boolean;
  explorerSelection: Selection | null;
  onExplorerBack: () => void;
  onExplorerPopOut: () => void;
  client: Client | null;
  pinnedObjects: ReadonlyMap<string, PinnedObject>;
}) {
  // Plain conditional (no animated transition); CSS/WAAPI/rAF attempts all failed back
  // transitions intermittently while the explorer was receiving live data.
  return (
    <div style={{ height: "100%", width: "100%" }}>
      {showExplorer && explorerSelection ? (
        <ObjectExplorerInPane
          client={client}
          selection={explorerSelection}
          onBack={onExplorerBack}
          onPopOut={onExplorerPopOut}
        />
      ) : (
        <InterestsPane client={client} pinnedObjects={pinnedObjects} />
      )}
    </div>
  );
}

function ObjectExplorerInPane({
  client,
  selection,
  onBack,
  onPopOut,
}: {
  client: Client | null;
  selection: Selection;
  onBack: () => void;
  onPopOut: () => void;
}) {
  const swatch = classSwatch(selection.className);
  return (
    <CurrentExplorerProvider selection={selection}>
      <div style={{ display: "flex", flexDirection: "column", height: "100%", fontSize: "var(--fs-lg)" }}>
        <div
          className="carved-bottom"
          style={{
            display: "flex",
            alignItems: "stretch",
            gap: 8,
            borderTop: `2px solid ${swatch.accent}`,
            background:
              "linear-gradient(180deg, rgba(255, 255, 255, 0.035) 0%, rgba(255, 255, 255, 0) 60%), var(--bg-elevated)",
            flex: "none",
          }}
        >
          <ExtendedBackButton onClick={onBack} />
          <div
            style={{
              display: "flex",
              alignItems: "center",
              gap: 8,
              padding: "6px 10px 6px 0",
              flex: 1,
              minWidth: 0,
            }}
          >
            <ExplorerHeaderTitle client={client} selection={selection} />
            <HoverIconButton
              ariaLabel="pop out"
              tooltip="Pop out to its own window"
              onClick={onPopOut}
              icon={<PopOutIcon />}
            />
          </div>
        </div>
        {/* DetailPane owns its own scroll boundary so tabs + filter strip stay pinned. */}
        <div style={{ flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }}>
          <DetailPane client={client} selection={selection} />
        </div>
      </div>
    </CurrentExplorerProvider>
  );
}

// Module-scoped watch_plot store crosses the portal cleanly (not React context).
export function ObjectExplorerPopout({
  client,
  selection,
}: {
  client: Client | null;
  selection: Selection;
}) {
  const swatch = classSwatch(selection.className);
  return (
    <CurrentExplorerProvider selection={selection}>
      <div
        style={{
          display: "flex",
          flexDirection: "column",
          height: "100vh",
          background: "var(--surface-pane)",
          backdropFilter: "var(--surface-blur)",
          WebkitBackdropFilter: "var(--surface-blur)",
          color: "var(--fg-base)",
        }}
      >
        <div
          style={{
            padding: "6px 10px",
            borderBottom: "1px solid var(--border-default)",
            borderTop: `2px solid ${swatch.accent}`,
            background:
              "linear-gradient(180deg, rgba(255, 255, 255, 0.035) 0%, rgba(255, 255, 255, 0) 60%), var(--bg-elevated)",
            boxShadow: "inset 0 -1px 0 rgba(255, 255, 255, 0.03)",
            display: "flex",
            alignItems: "center",
            gap: 8,
            flex: "none",
          }}
        >
          <ExplorerHeaderTitle client={client} selection={selection} />
        </div>
        <div style={{ flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }}>
          <DetailPane client={client} selection={selection} />
        </div>
      </div>
    </CurrentExplorerProvider>
  );
}

function ExtendedBackButton({ onClick }: { onClick: () => void }) {
  const [hover, setHover] = useState(false);
  return (
    <button
      type="button"
      onClick={onClick}
      title="Back to queries"
      aria-label="back to queries"
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        width: 40,
        minHeight: 32,
        padding: 0,
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        border: "none",
        borderRight: "1px solid var(--border-default)",
        borderRadius: 0,
        background: hover ? "var(--accent-wash)" : "transparent",
        color: hover ? "var(--accent-text-wash)" : "var(--fg-subtle)",
        cursor: "pointer",
        flex: "none",
        transition: "background 120ms ease, color 120ms ease",
      }}
    >
      <BackIcon />
    </button>
  );
}

function ExplorerHeaderTitle({ client, selection }: { client: Client | null; selection: Selection }) {
  // Composite key so same-class instance switches also trigger the slide.
  const selKey = `${selection.sessionName}.${selection.busName}.${selection.objectName}`;
  return (
    <SlidingHeaderBlock identity={selKey}>
      <span
        style={{
          color: "var(--fg-base)",
          fontSize: 17,
          lineHeight: 1.25,
          fontWeight: 500,
          fontFamily: "var(--font-mono)",
          whiteSpace: "nowrap",
          overflow: "hidden",
          textOverflow: "ellipsis",
          minWidth: 0,
          display: "block",
        }}
      >
        {selection.objectName}
      </span>
      <span
        style={{
          display: "inline-flex",
          alignItems: "center",
          gap: 6,
          minWidth: 0,
        }}
      >
        <span style={{ display: "inline-flex", minWidth: 0, overflow: "hidden", flexShrink: 0 }}>
          <Tooltip content={client?.getType(selection.className)?.description}>
            <span style={{ display: "inline-flex" }}>
              <TypeChip client={client} type={selection.className} />
            </span>
          </Tooltip>
        </span>
        <span style={{ display: "inline-flex", flex: "none" }}>
          <BusChip sessionName={selection.sessionName} busName={selection.busName} />
        </span>
      </span>
    </SlidingHeaderBlock>
  );
}

// Odometer-style slide on identity change; outgoing snapshot overlays absolutely so the
// current children can size the container in normal flow without shift.
const SLIDE_MS = 280;
function SlidingHeaderBlock({
  identity,
  children,
}: {
  identity: string;
  children: React.ReactNode;
}) {
  // Captured in a layoutEffect so the current children paint before the outgoing overlays.
  const [outgoing, setOutgoing] = useState<{ id: string; children: React.ReactNode } | null>(
    null,
  );
  const prevIdRef = useRef(identity);
  const prevChildrenRef = useRef(children);
  useLayoutEffect(() => {
    if (identity === prevIdRef.current) {
      prevChildrenRef.current = children;
      return;
    }
    setOutgoing({ id: prevIdRef.current, children: prevChildrenRef.current });
    prevIdRef.current = identity;
    prevChildrenRef.current = children;
    const tid = window.setTimeout(() => setOutgoing(null), SLIDE_MS);
    return () => window.clearTimeout(tid);
  }, [identity, children]);

  const innerLayout: React.CSSProperties = {
    display: "flex",
    flexDirection: "column",
    gap: 3,
    minWidth: 0,
  };
  return (
    <span
      style={{
        position: "relative",
        overflow: "hidden",
        display: "inline-flex",
        flexDirection: "column",
        minWidth: 0,
        flex: 1,
        fontFamily: "var(--font-ui)",
      }}
    >
      {outgoing && (
        <span
          key={`out-${outgoing.id}`}
          style={{
            ...innerLayout,
            position: "absolute",
            top: 0,
            left: 0,
            right: 0,
            pointerEvents: "none",
            animation: `webex-slide-up-out ${SLIDE_MS}ms ease both`,
          }}
        >
          {outgoing.children}
        </span>
      )}
      <span
        key={`in-${identity}`}
        style={{
          ...innerLayout,
          // Skip entry animation on initial mount; header should appear in place.
          animation: outgoing
            ? `webex-slide-up-in ${SLIDE_MS}ms ease both`
            : "none",
        }}
      >
        {children}
      </span>
    </span>
  );
}
