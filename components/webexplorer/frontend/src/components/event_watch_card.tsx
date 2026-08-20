// === EventWatchCard.tsx ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo, useRef } from "react";
import type * as React from "react";

import type { ArgSpec, Client, EventSpec } from "@sen/client";

import { formatTimestamp } from "../core/format.js";
import {
  makeEventWatchKey,
  type EventWatchSource,
} from "../core/watch_keys.js";
import { useClassEvents } from "../state/class_members.js";
import {
  eventWatchActions,
} from "../state/event_watches.js";
import { useObjectEvents, type EventDelivery } from "../state/event_store.js";
import {
  eventsDrawerTabsActions,
  useIsEventVisibleInTable,
} from "../state/events_drawer_tabs.js";
import { bottomPaneActions, bottomTabActions } from "../state/ui_prefs.js";
import { ObjectLink } from "../ui/explorer_links.js";
import { HoverIconButton, IconToggle } from "../ui/buttons.js";
import { LeftTruncated } from "../ui/text_primitives.js";
import {
  ChevronDownIcon,
  ChevronRightIcon,
  CloseIcon,
  EventsIcon,
  ICON_BUTTON_SIZE,
  TableIcon,
} from "../ui/icons.js";
import { ValueRenderer } from "../widgets/value_render/index.js";
import { ValueRow, useFlash } from "../widgets/value_render/value_row.js";

export function EventWatchCard({
  client,
  source,
  modeOverridden,
  onToggleModeOverride,
}: {
  client: Client | null;
  source: EventWatchSource;
  modeOverridden: boolean;
  onToggleModeOverride: () => void;
}) {
  // Tail-scan: steady state is O(1) since the match is usually the last entry.
  const log = useObjectEvents(source.interestName, source.objectName);
  const last = useMemo<EventDelivery | undefined>(() => {
    const events = log.events;
    for (let i = events.length - 1; i >= 0; i--) {
      const d = events[i]!;
      if (d.eventName === source.eventName) return d;
    }
    return undefined;
  }, [log, source.eventName]);

  const watchKey = makeEventWatchKey(source);
  const tableParts = {
    interestName: source.interestName,
    className: source.className,
    eventName: source.eventName,
  };
  const inTable = useIsEventVisibleInTable(tableParts, source.objectName);

  const onToggleTable = (): void => {
    if (inTable) {
      eventsDrawerTabsActions.removeSourceFromTab(tableParts, source.objectName);
      return;
    }
    eventsDrawerTabsActions.openType(tableParts, { ensureSourceVisible: source.objectName });
    bottomPaneActions.setFolded(false);
    bottomTabActions.set("events");
  };
  const onRemove = (): void => eventWatchActions.removeWatch(watchKey);

  // Mount-time gate so an existing tail delivery doesn't flash on first render.
  const mountTimeRef = useRef(Date.now());
  const flashSeq = last !== undefined && last.at >= mountTimeRef.current ? last.seq : undefined;
  // 500ms hold so closely spaced events overlap into one sustained glow.
  const hostRef = useRef<HTMLDivElement | null>(null);
  useFlash(flashSeq, 500, hostRef);
  const flashCls = "value-text";

  return (
    <div
      ref={hostRef}
      className="flash-cascade"
      style={{
        background: "var(--surface-card)",
        border: "1px solid var(--border-glass)",
        borderRadius: "var(--radius-lg)",
        boxShadow: "inset 0 1px 0 var(--surface-glass-highlight)",
        padding: modeOverridden ? "3px 6px 5px 4px" : "2px 6px 2px 4px",
        display: "flex",
        flexDirection: "column",
        gap: modeOverridden ? 4 : 0,
        alignItems: "stretch",
        position: "relative",
      }}
    >
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 5,
          width: "100%",
          minHeight: ICON_BUTTON_SIZE,
        }}
      >
        <HoverIconButton
          onClick={onToggleModeOverride}
          ariaLabel={modeOverridden ? "compact card" : "expand card"}
          tooltip={modeOverridden ? "Compact" : "Expand"}
          icon={modeOverridden ? <ChevronDownIcon /> : <ChevronRightIcon />}
          size={16}
        />
        {/* Flashes via filter:drop-shadow; text-shadow can't reach SVG strokes. */}
        <span
          className={flashCls}
          style={{
            display: "inline-flex",
            color: "var(--fg-subtle)",
            flex: "none",
          }}
          title="event"
        >
          <EventsIcon />
        </span>
        <ObjectLink
          selection={{
            interestName: source.interestName,
            objectName: source.objectName,
            className: source.className,
            sessionName: source.sessionName,
            busName: source.busName,
          }}
          title={`${source.objectName}.${source.eventName} on ${source.interestName}`}
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            fontWeight: 500,
            color: "var(--fg-base)",
            flex: "1 1 auto",
            minWidth: 0,
            maxWidth: "60%",
            display: "block",
            overflow: "hidden",
          }}
        >
          <LeftTruncated text={source.eventName} className={flashCls} />
        </ObjectLink>
        <EventInlineValue delivery={last} flashCls={flashCls} />
        <span
          style={{
            marginLeft: "auto",
            display: "inline-flex",
            alignItems: "center",
            gap: 5,
            flex: "none",
          }}
        >
          <IconToggle
            pressed={inTable}
            onClick={onToggleTable}
            tooltip={inTable ? "Hide from Events table" : "Show in Events table"}
            icon={<TableIcon />}
          />
          <HoverIconButton
            onClick={onRemove}
            ariaLabel={`remove watch for ${source.objectName}.${source.eventName}`}
            tooltip="Remove from Watch"
            icon={<CloseIcon />}
            danger
          />
        </span>
      </div>
      {modeOverridden && client && last && (
        <EventArgsTreeConnected client={client} source={source} delivery={last} />
      )}
      {modeOverridden && !last && (
        <span
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            color: "var(--fg-subtle)",
            paddingLeft: 24,
          }}
        >
          no deliveries yet.
        </span>
      )}
    </div>
  );
}

// Flash state owned by the parent so icon/name/timestamp pulse on one timer (independent
// timers would stagger).
function EventInlineValue({
  delivery,
  flashCls,
}: {
  delivery: EventDelivery | undefined;
  flashCls: string;
}) {
  return (
    <span
      className={flashCls}
      style={{
        flex: 1,
        minWidth: 0,
        paddingLeft: 6,
        textAlign: "right",
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-sm)",
        color: "var(--fg-subtle)",
        overflow: "hidden",
        whiteSpace: "nowrap",
        textOverflow: "ellipsis",
      }}
      title={delivery ? `${delivery.timestamp} UTC (server)` : undefined}
    >
      {delivery ? formatTimestamp(delivery.timestamp) : "--"}
    </span>
  );
}

function EventArgsTreeConnected({
  client,
  source,
  delivery,
}: {
  client: Client;
  source: EventWatchSource;
  delivery: EventDelivery;
}) {
  const allEvents = useClassEvents(client, source.className);
  const spec: EventSpec | undefined = useMemo(
    () => allEvents.find((e) => e.name === source.eventName),
    [allEvents, source.eventName],
  );
  if (!spec || spec.args.length === 0) {
    return (
      <span
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-md)",
          color: "var(--fg-subtle)",
          paddingLeft: 24,
        }}
      >
        no args.
      </span>
    );
  }
  return (
    <div
      style={{
        marginLeft: 2,
        paddingLeft: 8,
        borderLeft: "1px solid var(--border-default)",
        display: "grid",
        gridTemplateColumns: "auto minmax(0, 1fr)",
        rowGap: 2,
        alignItems: "baseline",
      }}
    >
      {spec.args.map((arg: ArgSpec, i: number) => {
        const v = delivery.args[i];
        return (
          <ValueRow key={arg.name} flashKey={v}>
            <>
              <span style={argNameStyle}>{arg.name}</span>
              <span
                className="value-cell"
                style={{ display: "inline-flex", minWidth: 0, alignItems: "center" }}
              >
                <span className="value-text">
                  <ValueRenderer client={client} declaredType={arg.type} value={v} />
                </span>
              </span>
            </>
          </ValueRow>
        );
      })}
    </div>
  );
}

const argNameStyle: React.CSSProperties = {
  fontFamily: "var(--font-mono)",
  fontSize: "var(--fs-md)",
  color: "var(--fg-muted)",
  whiteSpace: "nowrap",
  overflow: "hidden",
  textOverflow: "ellipsis",
  maxWidth: 200,
};
