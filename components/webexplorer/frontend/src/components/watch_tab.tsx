// === WatchTab.tsx ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { Client } from "@sen/client";

import { eventWatchActions, useEventWatches } from "../state/event_watches.js";
import { useWatchSources, watchActions } from "../state/watch_plot.js";
import { DangerPillButton } from "../ui/buttons.js";
import { EmptyPane, Mono } from "../ui/empty_state.js";
import { ObjectGroupedGrid } from "./watch_groups.js";

export interface WatchTabProps {
  client: Client | null;
}

// Key in the override set means "render in the OTHER mode" vs. the type-driven default
// (scalars default to compact, composites default to expanded sub-leaf tree).
const MODE_OVERRIDE_KEY = "webex.watchModeOverride";
const COLLAPSED_GROUPS_KEY = "webex.watchGroupsCollapsed";

function loadCollapsedFromKey(key: string): Set<string> {
  try {
    const raw = localStorage.getItem(key);
    if (!raw) return new Set();
    const arr = JSON.parse(raw);
    return new Set(Array.isArray(arr) ? arr.filter((s): s is string => typeof s === "string") : []);
  } catch {
    return new Set();
  }
}

export function WatchTab({ client }: WatchTabProps) {
  const watchSources = useWatchSources();
  const eventSources = useEventWatches();
  const [modeOverrideKeys, setModeOverrideKeys] = useState<Set<string>>(() =>
    loadCollapsedFromKey(MODE_OVERRIDE_KEY),
  );
  useEffect(() => {
    try {
      localStorage.setItem(MODE_OVERRIDE_KEY, JSON.stringify(Array.from(modeOverrideKeys)));
    } catch {
    }
  }, [modeOverrideKeys]);
  const toggleModeOverride = (key: string) => {
    setModeOverrideKeys((prev) => {
      const next = new Set(prev);
      if (next.has(key)) next.delete(key);
      else next.add(key);
      return next;
    });
  };
  const [collapsedGroupKeys, setCollapsedGroupKeys] = useState<Set<string>>(() =>
    loadCollapsedFromKey(COLLAPSED_GROUPS_KEY),
  );
  useEffect(() => {
    try {
      localStorage.setItem(COLLAPSED_GROUPS_KEY, JSON.stringify(Array.from(collapsedGroupKeys)));
    } catch {
    }
  }, [collapsedGroupKeys]);
  const toggleGroupCollapsed = (key: string) => {
    setCollapsedGroupKeys((prev) => {
      const next = new Set(prev);
      if (next.has(key)) next.delete(key);
      else next.add(key);
      return next;
    });
  };
  const totalSources = watchSources.length + eventSources.length;
  const clearAll = () => {
    watchActions.clearAllWatches();
    eventWatchActions.clearAllWatches();
  };
  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", minHeight: 0 }}>
      {totalSources > 0 && (
        <div
          style={{
            display: "flex",
            alignItems: "center",
            gap: 8,
            padding: "4px 10px",
            borderBottom: "1px solid var(--border-default)",
            flex: "none",
            background: "var(--surface-list-header)",
            fontFamily: "var(--font-ui)",
            fontSize: "var(--fs-xs)",
            color: "var(--fg-subtle)",
            letterSpacing: "0.06em",
            textTransform: "uppercase",
          }}
        >
          <span>{totalSources} watched</span>
          <span style={{ flex: 1 }} />
          <DangerPillButton
            onClick={clearAll}
            title="Remove every watch and event-watch from this pane"
          />
        </div>
      )}
      <div style={{ flex: 1, overflow: "auto" }}>
        {watchSources.length === 0 && eventSources.length === 0 ? (
          <EmptyPane
            heading="No sources watched yet."
            help={
              <>
                Click <Mono>+ Watch</Mono> next to a property OR event in the Object
                Explorer to add a source. <Mono>+plot</Mono> on a numeric leaf promotes
                it to the Plots drawer; <Mono>+Table</Mono> on an event opens its per-type
                table in the Events drawer.
              </>
            }
          />
        ) : (
          <ObjectGroupedGrid
            client={client}
            sources={watchSources}
            eventSources={eventSources}
            modeOverrideKeys={modeOverrideKeys}
            toggleModeOverride={toggleModeOverride}
            collapsedGroupKeys={collapsedGroupKeys}
            toggleGroupCollapsed={toggleGroupCollapsed}
          />
        )}
      </div>
    </div>
  );
}
