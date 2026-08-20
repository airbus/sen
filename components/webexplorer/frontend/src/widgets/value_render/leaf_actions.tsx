// === leaf_actions.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { memo, useMemo } from "react";
import type * as React from "react";

import type { Client, Var } from "@sen/client";
import { Quantity, Variant } from "@sen/client";

import type { PanelKind } from "../../core/panels.js";
import {
  makePlotKeyFromSource,
  makeWatchKey,
  type WatchSource,
} from "../../core/watch_keys.js";
import {
  overviewFavoritesActions,
  useIsOverviewFavorite,
} from "../../state/overview_favorites.js";
import { useIsPlotted, useIsWatched } from "../../state/watch_plot.js";
import { IconToggle } from "../../ui/buttons.js";
import { PlotIcon, StarIcon, WatchIcon } from "../../ui/icons.js";

// Returns null for composites + types whose leaves live below (TimeStamp, Duration, struct).
export function rootPlotKind(
  value: Var | undefined,
  declaredType: string,
  client: Client | null,
): PanelKind | null {
  if (value === undefined || value === null) return null;
  if (value instanceof Quantity) return "numeric";
  if (value instanceof Variant) return "discrete";
  if (typeof value === "number") return "numeric";
  if (typeof value === "boolean") return "discrete";
  if (typeof value === "string") {
    if (declaredType === "string") return "discrete";
    if (!client) return null;
    return stringPlotKindFor(client, declaredType);
  }
  return null;
}

// Invalidate on onTypeAdded so a late-arriving enum spec isn't stuck negative-cached.
const stringKindCache = new WeakMap<Client, Map<string, PanelKind | null>>();
const wiredTypeInvalidation = new WeakSet<Client>();

function stringPlotKindFor(client: Client, declaredType: string): PanelKind | null {
  let perClient = stringKindCache.get(client);
  if (!perClient) {
    perClient = new Map();
    stringKindCache.set(client, perClient);
  }
  if (!wiredTypeInvalidation.has(client)) {
    wiredTypeInvalidation.add(client);
    client.onTypeAdded(() => {
      stringKindCache.get(client)?.clear();
    });
  }
  const cached = perClient.get(declaredType);
  if (cached !== undefined) return cached;
  const spec = client.getType(declaredType);
  const kind: PanelKind | null =
    spec?.data.type === "sen.kernel.EnumTypeSpec" ? "discrete" : null;
  perClient.set(declaredType, kind);
  return kind;
}

export function PlotToggleButton({
  plotted,
  onClick,
}: {
  plotted: boolean;
  onClick: () => void;
}) {
  return (
    <IconToggle
      pressed={plotted}
      onClick={onClick}
      tooltip={plotted ? "Remove from Plots" : "Add to Plots"}
      icon={<PlotIcon />}
      palette="violet"
    />
  );
}

export function WatchToggleButton({
  watched,
  onClick,
  tooltip,
}: {
  watched: boolean;
  onClick: () => void;
  tooltip: string;
}) {
  return (
    <IconToggle
      pressed={watched}
      onClick={onClick}
      tooltip={tooltip}
      icon={<WatchIcon />}
      palette="green"
    />
  );
}

export type RenderLeafActions = (path: string, value: Var, declaredType: string) => React.ReactNode;

// Self-subscribed per-leaf; takes derived kind (not value) so delivery churn doesn't re-render.
export const LeafActions = memo(LeafActionsImpl);

interface LeafActionsProps {
  source: WatchSource;
  path: string;
  kind: PanelKind | null;
  onTogglePlottedLeaf: (source: WatchSource, leafPath: string, kind: PanelKind) => void;
  onToggleLeafWatch?: ((path: string) => void) | undefined;
}

function LeafActionsImpl({
  source,
  path,
  kind,
  onTogglePlottedLeaf,
  onToggleLeafWatch,
}: LeafActionsProps): React.ReactNode {
  const plotKey = useMemo(
    () => (kind === null ? null : makePlotKeyFromSource(source, path)),
    [kind, source, path],
  );
  const leafKey = useMemo(() => makeWatchKey({ ...source, leafPath: path }), [source, path]);
  const wholeKey = useMemo(() => makeWatchKey({ ...source, leafPath: "" }), [source]);
  const plotted = useIsPlotted(plotKey);
  // Pressed if THIS leaf OR the whole property is watched.
  const leafWatched = useIsWatched(leafKey);
  const wholeWatched = useIsWatched(wholeKey);
  const watched = leafWatched || wholeWatched;
  const showWatchIcon = onToggleLeafWatch !== undefined;
  const favorited = useIsOverviewFavorite(source.className, source.propertyName, path);
  // Render dim slot when non-plottable to keep the 3-icon grid aligned.
  return (
    <>
      {kind ? (
        <PlotToggleButton
          plotted={plotted}
          onClick={() => onTogglePlottedLeaf(source, path, kind)}
        />
      ) : (
        <DisabledPlotSlot />
      )}
      {showWatchIcon && (
        <WatchToggleButton
          watched={watched}
          onClick={() => onToggleLeafWatch(path)}
          tooltip={watched ? "Remove from Watch" : "Add to Watch"}
        />
      )}
      <IconToggle
        pressed={favorited}
        onClick={() => overviewFavoritesActions.toggle(source.className, source.propertyName, path)}
        tooltip={favorited ? "Remove from Overview cards" : "Show on Overview cards"}
        icon={<StarIcon />}
        palette="warm"
      />
    </>
  );
}

function DisabledPlotSlot() {
  return (
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
  );
}


export function buildLeafActionsRenderer({
  source,
  onTogglePlottedLeaf,
  onToggleLeafWatch,
  client,
}: {
  source: WatchSource;
  onTogglePlottedLeaf: (source: WatchSource, leafPath: string, kind: PanelKind) => void;
  onToggleLeafWatch?: ((path: string) => void) | undefined;
  client: Client | null;
}): RenderLeafActions {
  return (path, value, declaredType) => (
    <LeafActions
      source={source}
      path={path}
      kind={rootPlotKind(value, declaredType, client)}
      onTogglePlottedLeaf={onTogglePlottedLeaf}
      onToggleLeafWatch={onToggleLeafWatch}
    />
  );
}
