// === app.tsx =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type * as React from "react";

import { useConnectionState, useNotificationsDropped } from "@sen/client/react";

import { AppShell } from "./components/app_shell.js";
import { BottomPane, type BottomPaneTab } from "./components/bottom_pane.js";
import { CenterPane } from "./components/center_pane.js";
import { ConnectionStatusBanner } from "./components/connection_status_banner.js";
import type { Selection } from "./state/selection.js";
import { NavSwitcher, ObjectExplorerPopout } from "./components/object_explorer_hosts.js";
import { EventsWorkspace } from "./components/events_workspace.js";
import { PlotsView } from "./components/plots_view.js";
import { HoverIconButton } from "./ui/buttons.js";
import { EventsIcon, PlotIcon, PopOutIcon, CollapseAllIcon, ExpandAllIcon, CloseIcon } from "./ui/icons.js";
import { PopoutWindow } from "./components/popout_window.js";
import { SelectionCollectors } from "./components/selection_collectors.js";
import { SidebarHeader } from "./components/sidebar_header.js";
import { InterestOwner } from "./state/interest_owner.js";
import { Workspace } from "./components/workspace.js";
import { EventStoreProvider } from "./state/event_store.js";
import {
  explorerHostsActions,
  useInPaneExplorer,
  useLiveSelections,
  useOpenExplorers,
} from "./state/explorer_hosts.js";
import {
  plotsPopoutActions,
  usePlotsPopout,
  eventsPopoutActions,
  useEventsPopout,
  watchPopoutActions,
  useWatchPopout,
} from "./state/popouts.js";
import "./state/color_scheme.js";
import { SampleStoreProvider } from "./state/sample_store.js";
import { PopoutErrorBoundary } from "./ui/popout.js";
import { EventWatchCollectors } from "./components/event_watch_collectors.js";
import { WatchCollectors } from "./components/watch_collectors.js";
import { useEventRetentionSeconds, useSampleRetentionSeconds } from "./state/settings.js";
import {
  bottomPaneActions,
  bottomTabActions,
  useBottomFolded,
  useBottomHeight,
  useBottomTab,
  usePinnedObjects,
  useWorkspaceAnimating,
  useWorkspaceHidden,
  workspacePaneActions,
  useNavFolded,
  useNavAnimating,
  navPaneActions,
} from "./state/ui_prefs.js";
import { useConnection } from "./state/use_connection.js";
import { usePanels, usePlottedLeaves, useWatchSources, watchActions } from "./state/watch_plot.js";
import { WatchTab } from "./components/watch_tab.js";

function defaultUrl(): string {
  // Prod: same-origin (bundled SPA served by the JSON-RPC server hosting the WS).
  // Dev (Vite on :5173 etc.): point at the canonical local Sen port.
  // file:// and SSR fall through to the localhost default.
  if (import.meta.env.DEV) return "ws://127.0.0.1:8080";
  if (typeof window !== "undefined" && window.location.protocol.startsWith("http")) {
    const wsProto = window.location.protocol === "https:" ? "wss:" : "ws:";
    const hostname = window.location.hostname || "127.0.0.1";
    const port = window.location.port || (window.location.protocol === "https:" ? "443" : "80");
    return `${wsProto}//${hostname}:${port}`;
  }
  return "ws://127.0.0.1:8080";
}

export function App() {
  const { client, url, error, handleConnect, handleDisconnect, retryConnect } =
    useConnection(defaultUrl);

  const watchSources = useWatchSources();
  const panels = usePanels();
  const plottedLeaves = usePlottedLeaves();
  const inPaneExplorer = useInPaneExplorer();
  const liveSelections = useLiveSelections();
  const openExplorers = useOpenExplorers();
  const bottomFolded = useBottomFolded();
  const bottomHeight = useBottomHeight();
  const bottomTab = useBottomTab();
  const workspaceHidden = useWorkspaceHidden();
  const workspaceAnimating = useWorkspaceAnimating();
  const navFolded = useNavFolded();
  const navAnimating = useNavAnimating();
  const pinnedObjects = usePinnedObjects();

  const plotsPopout = usePlotsPopout();
  const eventsPopout = useEventsPopout();
  const watchPopout = useWatchPopout();

  const [dismissedError, setDismissedError] = useState<Error | null>(null);

  // Clear dismissed error if the connection error resolves itself
  useEffect(() => {
    if (!error) setDismissedError(null);
  }, [error]);

  // Auto-unfold on the 0 -> >=1 transition only; user folds stick. bottomFolded read
  // via ref so this effect runs only when panelCount changes.
  const panelCount = panels.length;
  const prevPanelCountRef = useRef(panelCount);
  const bottomFoldedRef = useRef(bottomFolded);
  bottomFoldedRef.current = bottomFolded;
  useEffect(() => {
    if (prevPanelCountRef.current === 0 && panelCount > 0 && bottomFoldedRef.current) {
      bottomPaneActions.setFolded(false);
    }
    prevPanelCountRef.current = panelCount;
  }, [panelCount]);

  const state = useConnectionState(client);
  const droppedNotifications = useNotificationsDropped(client);

  const prevStateRef = useRef(state);
  useEffect(() => {
    if (prevStateRef.current !== "open" && state === "open" && error) {
      setDismissedError(error);
    }
    prevStateRef.current = state;
  }, [state, error]);

  const navWasFoldedRef = useRef(false);

  const onOpenInPaneExplorer = useCallback(
    (sel: Selection) => {
      navWasFoldedRef.current = navFolded;
      if (navFolded) {
        navPaneActions.setFolded(false);
      }
      explorerHostsActions.openInPaneExplorer(sel);
    },
    [navFolded],
  );

  const onExplorerBack = useCallback(() => {
    explorerHostsActions.closeInPaneExplorer();
    if (navWasFoldedRef.current) {
      navPaneActions.setFolded(true);
      navWasFoldedRef.current = false;
    }
  }, []);

  const onExplorerPopOut = useCallback(() => {
    if (inPaneExplorer) {
      const sel = inPaneExplorer;
      explorerHostsActions.closeInPaneExplorer();
      explorerHostsActions.toggleExplorer(sel);
      if (navWasFoldedRef.current) {
        navPaneActions.setFolded(true);
        navWasFoldedRef.current = false;
      }
    }
  }, [inPaneExplorer]);

  // Stable identity so BottomPane prop doesn't flip every render.
  const onToggleFolded = useCallback(
    () => bottomPaneActions.setFolded((v) => !v),
    [],
  );

  // AppShell is memoized; each slot lists only its real deps so unrelated state
  // changes don't invalidate the others.
  const nav = useMemo(
    () => (
      <div
        style={{
          display: "flex",
          flexDirection: "column",
          gap: "var(--pane-gutter-top)",
          height: "100%",
          minHeight: 0,
        }}
      >
        <div
          style={{
            height: "var(--topbar-height)",
            flex: "none",
            borderRadius: "var(--radius-xl)",
            border: "1px solid var(--border-glass)",
            background: "var(--surface-pane-nav)",
            boxShadow: "var(--surface-shadow), var(--surface-glass-bevel)",
            overflow: "hidden",
            display: "flex",
            alignItems: "center",
          }}
        >
          <SidebarHeader folded={navFolded} />
        </div>
        <div
          style={{
            flex: 1,
            minHeight: 0,
            borderRadius: "var(--radius-xl)",
            border: "1px solid var(--border-glass)",
            background: "var(--surface-pane-nav)",
            boxShadow: "var(--surface-shadow), var(--surface-glass-bevel)",
            overflow: "hidden",
            display: "flex",
            flexDirection: "column",
          }}
        >
          <div style={{ flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }}>
            <NavSwitcher
              showExplorer={!!inPaneExplorer}
              explorerSelection={inPaneExplorer}
              onExplorerBack={onExplorerBack}
              onExplorerPopOut={onExplorerPopOut}
              client={client}
              pinnedObjects={pinnedObjects}
              folded={navFolded}
            />
          </div>
          <div
            style={{
              borderTop: "1px solid var(--border-subtle)",
              padding: "6px",
              display: "flex",
              justifyContent: navFolded ? "center" : "flex-end",
              flex: "none",
            }}
          >
            <HoverIconButton
              onClick={navPaneActions.toggle}
              icon={
                navFolded ? (
                  <span style={{ transform: "rotate(-90deg)", display: "inline-flex" }}>
                    <ExpandAllIcon />
                  </span>
                ) : (
                  <span style={{ transform: "rotate(-90deg)", display: "inline-flex" }}>
                    <CollapseAllIcon />
                  </span>
                )
              }
              ariaLabel={navFolded ? "Expand navigation sidebar" : "Collapse navigation sidebar"}
              tooltip={navFolded ? "Expand sidebar" : "Collapse sidebar"}
              size={26}
            />
          </div>
        </div>
      </div>
    ),
    [inPaneExplorer, onExplorerPopOut, client, pinnedObjects, navFolded],
  );
  const bottomTabs = useMemo<readonly BottomPaneTab[]>(() => {
    const tabs: BottomPaneTab[] = [];
    if (!plotsPopout) {
      tabs.push({
        key: "plots",
        title: "Plots",
        icon: <PlotIcon />,
        count: plottedLeaves.size,
        trailing: (
          <HoverIconButton
            onClick={plotsPopoutActions.open}
            tooltip="Open plots in a separate window"
            ariaLabel="pop out plots"
            icon={<PopOutIcon />}
            size={20}
          />
        ),
        content: (
          <PlotsView
            client={client}
            panels={panels}
            onRemoveSeries={watchActions.removeSeries}
            onMoveSeriesToPanel={watchActions.moveSeriesToPanel}
            onMoveSeriesToNewPanel={watchActions.moveSeriesToNewPanel}
          />
        ),
      });
    }
    if (!eventsPopout) {
      tabs.push({
        key: "events",
        title: "Events",
        icon: <EventsIcon />,
        trailing: (
          <HoverIconButton
            onClick={eventsPopoutActions.open}
            tooltip="Open events in a separate window"
            ariaLabel="pop out events"
            icon={<PopOutIcon />}
            size={20}
          />
        ),
        content: <EventsWorkspace client={client} />,
      });
    }
    return tabs;
  }, [client, panels, plottedLeaves, plotsPopout, eventsPopout]);

  const detail = useMemo(
    () => (
      <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
        {/* Left padding 0 so the workspace top band aligns with the sidebar's right
            edge (sidebar owns that gutter). */}
        <div style={{ flex: 1, minHeight: 0, overflow: "hidden", padding: "var(--pane-gutter-top) var(--pane-gutter-side) 0 0" }}>
          <CenterPane
            client={client}
            onOpenInPaneExplorer={onOpenInPaneExplorer}
            banner={
              error && error !== dismissedError ? (
                <ErrorBanner
                  message={error.message}
                  onDismiss={() => setDismissedError(error)}
                />
              ) : null
            }
          />
        </div>
        {bottomTabs.length > 0 && (
          <BottomPane
            tabs={bottomTabs}
            activeTabKey={bottomTab}
            onActiveTabChange={bottomTabActions.set}
            folded={bottomFolded}
            onToggleFolded={onToggleFolded}
            height={bottomHeight}
            onResize={bottomPaneActions.setHeight}
            onResizeCommit={bottomPaneActions.commitHeight}
            marginLeft={0}
            marginRight={14}
          />
        )}
      </div>
    ),
    [
      client,
      error,
      dismissedError,
      bottomTabs,
      bottomTab,
      bottomFolded,
      bottomHeight,
      onToggleFolded,
    ],
  );
  const workspace = useMemo(
    () =>
      ({ onResize, onResizeEnd }: { onResize: (dx: number) => void; onResizeEnd: () => void }) => (
        <Workspace
          client={client}
          folded={workspaceHidden}
          onToggleFolded={workspacePaneActions.toggle}
          onResize={onResize}
          onResizeEnd={onResizeEnd}
        />
      ),
    [client, workspaceHidden],
  );
  const popouts = useMemo(
    () =>
      Array.from(openExplorers.entries()).map(([key, entry]) => (
        <PopoutWindow
          key={key}
          handle={entry.handle}
          onClose={() => explorerHostsActions.closeExplorerByKey(key)}
        >
          <ObjectExplorerPopout client={client} selection={entry.selection} />
        </PopoutWindow>
      )),
    [openExplorers, client],
  );

  return (
    <StoreProviders>
      <InterestOwner client={client} />
      <WatchCollectors sources={watchSources} />
      <EventWatchCollectors client={client} />
      <SelectionCollectors client={client} selections={liveSelections} />
      {popouts}

      {plotsPopout && (
        <PopoutWindow handle={plotsPopout} onClose={plotsPopoutActions.clearOnUnload}>
          {/* Drag/resize of gridstack panels is auto-disabled in the popup; gridstack's
                mouse handlers don't cross documents. Plot data + uPlot pan/zoom still work. */}
          <div style={{ height: "100%", display: "flex", flexDirection: "column" }}>
            <div
              style={{
                padding: "3px 10px",
                fontFamily: "var(--font-ui)",
                fontSize: "var(--fs-xs)",
                color: "var(--fg-subtle)",
                background: "var(--bg-elevated)",
                borderBottom: "1px solid var(--border-subtle)",
                flex: "none",
              }}
            >
              Layout locked - rearrange panels in the main window.
            </div>
            <PopoutErrorBoundary>
              <PlotsView
                client={client}
                panels={panels}
                onRemoveSeries={watchActions.removeSeries}
                onMoveSeriesToPanel={watchActions.moveSeriesToPanel}
                onMoveSeriesToNewPanel={watchActions.moveSeriesToNewPanel}
              />
            </PopoutErrorBoundary>
          </div>
        </PopoutWindow>
      )}

      {eventsPopout && (
        <PopoutWindow handle={eventsPopout} onClose={eventsPopoutActions.clearOnUnload}>
          <div style={{ height: "100%", display: "flex", flexDirection: "column" }}>
            <PopoutErrorBoundary>
              <EventsWorkspace client={client} />
            </PopoutErrorBoundary>
          </div>
        </PopoutWindow>
      )}

      {watchPopout && (
        <PopoutWindow handle={watchPopout} onClose={watchPopoutActions.clearOnUnload}>
          <div style={{ height: "100%", display: "flex", flexDirection: "column" }}>
            <PopoutErrorBoundary>
              <WatchTab client={client} />
            </PopoutErrorBoundary>
          </div>
        </PopoutWindow>
      )}

      <div style={{ height: "100%", display: "flex", flexDirection: "column" }}>
        <ConnectionStatusBanner
          state={state}
          url={url}
          onRetry={retryConnect}
          droppedNotifications={droppedNotifications}
        />
        {/* Offline: dim data surfaces; banner stays full-color so Retry is visible. */}
        <div
          style={{
            flex: 1,
            minHeight: 0,
            display: "flex",
            flexDirection: "column",
            filter: state === "open" ? "none" : "grayscale(0.75) opacity(0.72)",
            transition: "filter 260ms ease",
          }}
        >
          <AppShell
            nav={nav}
            detail={detail}
            workspace={workspace}
            wsHidden={workspaceHidden}
            wsAnimating={workspaceAnimating}
            wsRemoved={!!watchPopout}
            navFolded={navFolded}
            navAnimating={navAnimating}
          />
        </div>
      </div>
    </StoreProviders>
  );
}

function ErrorBanner({ message, onDismiss }: { message: string; onDismiss: () => void }) {
  return (
    <div
      style={{
        margin: "8px 12px",
        padding: "8px 12px",
        color: "var(--err)",
        border: "1px solid var(--err)",
        borderRadius: "var(--radius-sm)",
        background: "var(--bg-elevated)",
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-md)",
        display: "flex",
        alignItems: "center",
        justifyContent: "space-between",
        gap: 12,
      }}
    >
      <span style={{ minWidth: 0, overflowWrap: "break-word" }}>{message}</span>
      <HoverIconButton
        onClick={onDismiss}
        icon={<CloseIcon />}
        ariaLabel="dismiss error"
        tooltip="Dismiss"
        danger
      />
    </div>
  );
}

function StoreProviders({ children }: { children: React.ReactNode }) {
  const sampleRetention = useSampleRetentionSeconds();
  const eventRetention = useEventRetentionSeconds();
  return (
    <SampleStoreProvider retentionSeconds={sampleRetention}>
      <EventStoreProvider retentionSeconds={eventRetention}>{children}</EventStoreProvider>
    </SampleStoreProvider>
  );
}
