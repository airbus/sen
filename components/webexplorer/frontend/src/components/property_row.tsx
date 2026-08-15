// === PropertyRow.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { memo, useCallback, useContext, useMemo, useRef } from "react";

import type { Client, ObjectHandle, PropertySpec } from "@sen/client";

import { isComposite } from "../core/types.js";
import { enumerateTopLevelPaths } from "../core/value_walk.js";
import {
  makePlotKeyFromSource,
  makePropertyKey,
  makeWatchKey,
  type WatchSource,
} from "../core/watch_keys.js";
import { useLiveProperty } from "../state/live_value.js";
import {
  overviewFavoritesActions,
  useIsOverviewFavorite,
} from "../state/overview_favorites.js";
import type { Selection } from "../state/selection.js";
import {
  getWatchedKeysForProperty,
  useHasAnyWatchOnProperty,
  useIsPlotted,
  useIsWatched,
  watchActions,
} from "../state/watch_plot.js";
import { Collapsible } from "../ui/layout.js";
import { HelpButton, IconToggle } from "../ui/buttons.js";

import { ChevronDownIcon, ChevronRightIcon, PlotIcon, StarIcon, WatchIcon } from "../ui/icons.js";
import { Tooltip } from "../ui/tooltip.js";
import { PropertyKindIconCell } from "./property_kind_icon_cell.js";
import { MetaRow } from "../ui/meta_row.js";
import { FieldWriter } from "../widgets/value_edit/index.js";
import {
  buildLeafActionsRenderer,
  PlotToggleButton,
  rootPlotKind,
  ValueRenderer,
  WatchToggleButton,
} from "../widgets/value_render/index.js";
import { ACTIONS_COLUMN_WIDTH } from "../widgets/value_render/struct_value.js";
import { useFlash, ValueFilterContext } from "../widgets/value_render/value_row.js";

export interface PropertyRowProps {
  client: Client;
  obj: ObjectHandle;
  property: PropertySpec;
  selection: Selection;
  collapsed: boolean;
  onToggleCollapsed: (propertyName: string) => void;
}

export const PropertyRow = memo(PropertyRowImpl);

function PropertyRowImpl({
  client,
  obj,
  property,
  selection,
  collapsed: collapsedProp,
  onToggleCollapsed,
}: PropertyRowProps) {
  // Under an active value-filter force-expand so the matching leaf is visible.
  const activeFilter = useContext(ValueFilterContext);
  const collapsed = activeFilter ? false : collapsedProp;
  const live = useLiveProperty(obj, property.name);
  const value = live.value;
  const rowRef = useRef<HTMLDivElement | null>(null);
  useFlash(value, undefined, rowRef);
  const source: WatchSource = useMemo(
    () => ({
      interestName: selection.interestName,
      objectName: selection.objectName,
      className: selection.className,
      propertyName: property.name,
      declaredType: property.type,
      sessionName: selection.sessionName,
      busName: selection.busName,
    }),
    [selection, property.name, property.type],
  );
  const wholeKey = useMemo(() => makeWatchKey({ ...source, leafPath: "" }), [source]);
  const rootPlotKey = useMemo(() => makePlotKeyFromSource(source, ""), [source]);
  const propertyKey = useMemo(() => makePropertyKey(source), [source]);
  const propertyHasWholeWatch = useIsWatched(wholeKey);
  const rootPlotted = useIsPlotted(rootPlotKey);
  const propertyHasAnyWatch = useHasAnyWatchOnProperty(propertyKey);
  // Collapses per-leaf entries into a whole-property watch (and back); reads leaf keys
  // at click time instead of subscribing.
  const togglePropertyWatch = useCallback((): void => {
    if (propertyHasWholeWatch) {
      watchActions.removeWatch(wholeKey);
      return;
    }
    const existing = getWatchedKeysForProperty(propertyKey);
    watchActions.batchEdit(existing, [{ ...source, leafPath: "" }]);
  }, [propertyHasWholeWatch, wholeKey, propertyKey, source]);
  // Three cases per click: inside whole-watch -> expand to N-1 siblings; individually
  // watched -> remove; unwatched -> add, collapsing to whole-property when the addition
  // completes the primary-child set.
  const onToggleLeafWatch = useCallback(
    (leafPath: string): void => {
      if (!leafPath) {
        togglePropertyWatch();
        return;
      }
      const leafSource: WatchSource = { ...source, leafPath };
      const leafKey = makeWatchKey(leafSource);
      const primaryChildren = enumerateTopLevelPaths(client, property.type) ?? [];
      if (propertyHasWholeWatch) {
        const additions = primaryChildren
          .filter((c) => c !== leafPath)
          .map((c) => ({ ...source, leafPath: c }));
        watchActions.batchEdit([wholeKey], additions);
        return;
      }
      const existing = getWatchedKeysForProperty(propertyKey);
      if (existing.includes(leafKey)) {
        watchActions.removeWatch(leafKey);
        return;
      }
      if (primaryChildren.length === 0) {
        watchActions.addWatch(leafSource);
        return;
      }
      const stillNeeded = primaryChildren.filter(
        (c) =>
          c !== leafPath &&
          !existing.includes(makeWatchKey({ ...source, leafPath: c })),
      );
      if (stillNeeded.length === 0) {
        const removals = primaryChildren.map((c) => makeWatchKey({ ...source, leafPath: c }));
        watchActions.batchEdit(removals, [{ ...source, leafPath: "" }]);
      } else {
        watchActions.addWatch(leafSource);
      }
    },
    [
      togglePropertyWatch,
      source,
      client,
      property.type,
      propertyHasWholeWatch,
      propertyKey,
      wholeKey,
    ],
  );
  const isStatic = property.category === "staticRW" || property.category === "staticRO";
  // Identity-stable so the closure threaded through nested struct/sequence/variant doesn't
  // defeat React.memo at any layer.
  const renderLeafActions = useMemo(
    () =>
      buildLeafActionsRenderer({
        source,
        onTogglePlottedLeaf: watchActions.togglePlottedLeaf,
        onToggleLeafWatch,
        client,
      }),
    [source, onToggleLeafWatch, client],
  );
  const valueRootKind = rootPlotKind(value, property.type, client);
  const showCornerPlot = valueRootKind !== null;
  // Only dynamicRW is runtime-writable.
  const writable = property.category === "dynamicRW";
  // Composite values render as header + full-width body; leaves stay inline. Undefined
  // values stay inline so the placeholder doesn't shift on first push.
  const blockLayout = value !== undefined && value !== null && isComposite(client, property.type);
  const nameCell = (
    <span style={{ display: "inline-flex", alignItems: "center", gap: 6, minWidth: 0 }}>
      <PropertyKindIconCell
        client={client}
        className={selection.className}
        propertyName={property.name}
      />
      <Tooltip content={property.description}>
        <span
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-lg)",
            color: "var(--fg-base)",
            maxWidth: 200,
            minWidth: 0,
            overflow: "hidden",
            textOverflow: "ellipsis",
            whiteSpace: "nowrap",
          }}
        >
          {property.name}
        </span>
      </Tooltip>
    </span>
  );
  const actions = (
    <>
      <HelpButton
        description={property.description}
        ariaLabel={`${property.name} documentation`}
      />
      {showCornerPlot ? (
        <PlotToggleButton
          plotted={rootPlotted}
          onClick={() => watchActions.togglePlottedLeaf(source, "", valueRootKind!)}
        />
      ) : (
        // Dim placeholder preserves column alignment with LeafActions.
        <span
          aria-hidden
          title="Not plottable"
          style={{
            width: 24,
            height: 24,
            display: "inline-flex",
            alignItems: "center",
            justifyContent: "center",
            flex: "none",
            color: "var(--fg-faint)",
            opacity: 0.4,
            pointerEvents: "none",
          }}
        >
          <PlotIcon />
        </span>
      )}
      <WatchToggleButton
        watched={propertyHasAnyWatch}
        onClick={togglePropertyWatch}
        tooltip={
          propertyHasWholeWatch
            ? "Remove from Watch"
            : propertyHasAnyWatch
              ? "Watch whole property (clears per-field watches)"
              : "Add to Watch"
        }
      />
      <OverviewFavoriteToggle
        className={selection.className}
        propertyName={property.name}
      />
    </>
  );
  const valueNode = writable ? (
    <FieldWriter
      client={client}
      obj={obj}
      propertyName={property.name}
      declaredType={property.type}
      value={value}
    />
  ) : (
    <ValueRenderer
      client={client}
      declaredType={property.type}
      value={value}
      renderLeafActions={renderLeafActions}
    />
  );
  // Composite shells defer hover + flash to per-leaf ValueRows; scalars own it on the row.
  const rowClass = blockLayout ? "property-row-shell carved-bottom" : "property-row carved-bottom";
  return (
    <div
      ref={rowRef}
      className={rowClass}
      style={{
        // Right padding 0: action icons are the right edge. Bottom 28 reserves space for
        // the absolutely-positioned MetaRow.
        padding: "8px 0 28px 12px",
        position: "relative",
      }}
    >
      {blockLayout ? (
        <>
          {/* Grid matches struct_value/sequence_value so action icons stack in one column. */}
          <div
            style={{
              display: "grid",
              gridTemplateColumns: `auto minmax(0, 1fr) ${ACTIONS_COLUMN_WIDTH}px`,
              columnGap: 8,
              alignItems: "center",
              cursor: "pointer",
            }}
            onClick={() => onToggleCollapsed(property.name)}
            role="button"
            aria-expanded={!collapsed}
          >
            <span
              style={{
                display: "inline-flex",
                alignItems: "center",
                gap: 6,
                minWidth: 0,
              }}
            >
              <span
                style={{
                  width: 11,
                  height: 11,
                  display: "inline-flex",
                  alignItems: "center",
                  justifyContent: "center",
                  color: "var(--fg-muted)",
                  flex: "0 0 auto",
                }}
              >
                {collapsed ? <ChevronRightIcon /> : <ChevronDownIcon />}
              </span>
              {nameCell}
            </span>
            <span aria-hidden />
            <span
              style={{
                display: "inline-flex",
                gap: 4,
                justifySelf: "end",
                alignItems: "center",
              }}
              onClick={(e) => e.stopPropagation()}
            >
              {actions}
            </span>
          </div>
          <Collapsible open={!collapsed}>
            <div style={{ marginTop: 4 }}>{valueNode}</div>
          </Collapsible>
        </>
      ) : (
        <div
          style={{
            display: "grid",
            gridTemplateColumns: `auto minmax(0, 1fr) ${ACTIONS_COLUMN_WIDTH}px`,
            columnGap: 12,
            alignItems: "center",
          }}
        >
          <span style={{ minWidth: 0 }}>{nameCell}</span>
          <div
            className="value-cell"
            style={{
              display: "flex",
              minWidth: 0,
              alignItems: "center",
              gap: 8,
            }}
          >
            <span className="value-text">{valueNode}</span>
          </div>
          <span
            style={{
              display: "inline-flex",
              gap: 4,
              justifySelf: "end",
              alignItems: "center",
            }}
          >
            {actions}
          </span>
        </div>
      )}
      <span style={{ position: "absolute", left: 12, bottom: 8 }}>
        <MetaRow
          client={client}
          declaredType={property.type}
          writable={writable}
          isStatic={isStatic}
          timestamp={live.timestamp ?? null}
        />
      </span>
    </div>
  );
}

// Class-scoped: marking Controller.power surfaces it on every Controller card.
function OverviewFavoriteToggle({
  className,
  propertyName,
}: {
  className: string;
  propertyName: string;
}) {
  const fav = useIsOverviewFavorite(className, propertyName);
  return (
    <IconToggle
      pressed={fav}
      onClick={() => overviewFavoritesActions.toggle(className, propertyName)}
      tooltip={fav ? "Remove from Overview cards" : "Show on Overview cards"}
      icon={<StarIcon />}
      palette="warm"
    />
  );
}
