// === PropertyTable.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo } from "react";

import type { Client, ObjectHandle } from "@sen/client";

import {
  makeWatchKey,
  type WatchSource,
} from "../core/watch_keys.js";
import { collectTypeSearchTerms } from "../core/value_walk.js";
import { type ClassMemberGroup } from "../state/class_members.js";
import type { Selection } from "../state/selection.js";
import { useWatchedKeys, watchActions } from "../state/watch_plot.js";
import { Collapsible } from "../ui/layout.js";
import { EmptyHint } from "../ui/empty_state.js";
import { WatchToggleButton } from "../widgets/value_render/index.js";
import { FlashEnabledContext, ValueFilterContext } from "../widgets/value_render/value_row.js";
import { GroupHeader } from "./group_header.js";
import { PropertyRow } from "./property_row.js";

export function PropertyTable({
  client,
  obj,
  groups,
  selection,
  filter,
  collapsedClasses,
  collapsedProperties,
  onToggleClassCollapsed,
  onTogglePropertyCollapsed,
}: {
  client: Client;
  obj: ObjectHandle;
  groups: ClassMemberGroup[];
  selection: Selection;
  filter: string;
  collapsedClasses: ReadonlySet<string>;
  collapsedProperties: ReadonlySet<string>;
  onToggleClassCollapsed: (className: string) => void;
  onTogglePropertyCollapsed: (propertyName: string) => void;
}) {
  const orderedGroups = useMemo(() => [...groups].reverse(), [groups]);
  const allProperties = useMemo(() => orderedGroups.flatMap((g) => g.properties), [orderedGroups]);
  const showGroupHeaders = orderedGroups.filter((g) => g.properties.length > 0).length > 1;
  // Search blob = name + declared type + every nested field/arm/enum reachable through the
  // type tree, so "velocity" surfaces a `spatial` whose `velocityVector` has `xVelocity`.
  // Built once per (groups, client) so keystroke cost stays at one .includes per property.
  const filterLower = filter.trim().toLowerCase();
  const searchBlobs = useMemo(() => {
    const map = new Map<string, string>();
    for (const g of orderedGroups) {
      for (const p of g.properties) {
        const terms: string[] = [p.name, p.type];
        collectTypeSearchTerms(client, p.type, terms);
        map.set(p.name, terms.join("\0").toLowerCase());
      }
    }
    return map;
  }, [orderedGroups, client]);
  const filteredGroups = useMemo(() => {
    if (!filterLower) return orderedGroups;
    return orderedGroups.map((g) => ({
      ...g,
      properties: g.properties.filter((p) =>
        (searchBlobs.get(p.name) ?? p.name.toLowerCase()).includes(filterLower),
      ),
    }));
  }, [orderedGroups, filterLower, searchBlobs]);
  const totalAfterFilter = useMemo(
    () => filteredGroups.reduce((sum, g) => sum + g.properties.length, 0),
    [filteredGroups],
  );

  // Opt into the value-change flash; other reuses of these value renderers leave it false.
  return (
    <FlashEnabledContext.Provider value={true}>
    <div>
      {allProperties.length === 0 ? (
        <EmptyHint>no properties</EmptyHint>
      ) : totalAfterFilter === 0 ? (
        <EmptyHint>no properties match the filter.</EmptyHint>
      ) : (
        filteredGroups.map((g) => {
          if (g.properties.length === 0) return null;
          const classCollapsed = collapsedClasses.has(g.className);
          return (
            <div
              key={g.className}
              style={
                showGroupHeaders
                  ? {
                      marginLeft: 4,
                      borderLeft: "2px solid rgba(180, 180, 200, 0.4)",
                      paddingLeft: 8,
                      marginBottom: 10,
                    }
                  : undefined
              }
            >
              {showGroupHeaders && (
                <GroupHeader
                  className={g.className}
                  collapsed={classCollapsed}
                  onToggle={() => onToggleClassCollapsed(g.className)}
                />
              )}
              <Collapsible open={!classCollapsed}>
                {g.properties.map((p) => {
                  // If the property name matched the filter, don't narrow leaves inside.
                  // If only a descendant matched, propagate the filter to narrow them.
                  const ownMatch =
                    filterLower.length > 0 && p.name.toLowerCase().includes(filterLower);
                  const subFilter = filterLower && !ownMatch ? filterLower : "";
                  return (
                    <ValueFilterContext.Provider value={subFilter} key={p.name}>
                      <PropertyRow
                        client={client}
                        obj={obj}
                        property={p}
                        selection={selection}
                        collapsed={collapsedProperties.has(p.name)}
                        onToggleCollapsed={onTogglePropertyCollapsed}
                      />
                    </ValueFilterContext.Provider>
                  );
                })}
              </Collapsible>
            </div>
          );
        })
      )}
    </div>
    </FlashEnabledContext.Provider>
  );
}

export function WatchAllToggle({
  groups,
  selection,
}: {
  groups: ClassMemberGroup[];
  selection: Selection;
}) {
  const watchedKeys = useWatchedKeys();
  const { sources, keys, allWatched } = useMemo(() => {
    const allProperties = groups.flatMap((g) => g.properties);
    const sources: WatchSource[] = allProperties.map((p) => ({
      interestName: selection.interestName,
      objectName: selection.objectName,
      className: selection.className,
      propertyName: p.name,
      declaredType: p.type,
      sessionName: selection.sessionName,
      busName: selection.busName,
    }));
    const keys = sources.map((s) => makeWatchKey(s));
    const allWatched = keys.length > 0 && keys.every((k) => watchedKeys.has(k));
    return { sources, keys, allWatched };
  }, [groups, selection, watchedKeys]);
  const toggle = () => {
    // Single batchEdit dispatch; per-key looping was dominant render cost on a 50-prop class.
    if (allWatched) {
      watchActions.batchEdit(keys, []);
    } else {
      const additions: WatchSource[] = [];
      for (let i = 0; i < sources.length; i++) {
        if (!watchedKeys.has(keys[i]!)) additions.push(sources[i]!);
      }
      watchActions.batchEdit([], additions);
    }
  };
  return (
    <WatchToggleButton
      watched={allWatched}
      onClick={toggle}
      tooltip={allWatched ? "Remove all from Watch" : "Add all to Watch"}
    />
  );
}
