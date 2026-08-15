// === ObjectsWorkspace.tsx ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useMemo, useRef, useState } from "react";
import type * as React from "react";

import type { Client } from "@sen/client";

import { inheritanceChain, useTypeCacheTick } from "../state/class_members.js";
import { useInPaneExplorer, useOpenExplorers } from "../state/explorer_hosts.js";
import { useAllInterests } from "../state/interest_registry.js";
import type { Selection } from "../state/selection.js";
import { pinnedObjectsActions, usePinnedObjects } from "../state/ui_prefs.js";
import { pinKey } from "../core/keys.js";
import { AnimatedGridItem, useAnimatedList } from "../ui/animated_list.js";
import { BusChip, TypeChip } from "../ui/chips.js";
import { CollapsibleSection } from "../ui/card_section.js";
import { classSwatch } from "../ui/class_color.js";
import { EmptyPane, Mono } from "../ui/empty_state.js";
import { FilterInput } from "../ui/filter_input.js";
import { HoverIconButton } from "../ui/buttons.js";
import { CollapseAllIcon, ExpandAllIcon } from "../ui/icons.js";
import { objectCardStyle } from "../ui/object_card_style.js";
import { SegmentedControl } from "../ui/segmented_control.js";
import { parseInterestName } from "../widgets/interests/bus_queries.js";

const GROUP_BY_KEY = "webex.objects.groupBy";
const FILTER_KEY = "webex.objects.filter";
const COLLAPSED_SECTIONS_KEY = "webex.objects.collapsedSections";

type GroupBy = "bus" | "class" | "query" | "flat";

interface ObjectsWorkspaceProps {
  client: Client | null;
  onOpenInPaneExplorer: (sel: Selection) => void;
}

export function ObjectsWorkspace({ client, onOpenInPaneExplorer }: ObjectsWorkspaceProps) {
  const [filter, setFilter] = useState<string>(() => loadString(FILTER_KEY, ""));
  const [groupBy, setGroupBy] = useState<GroupBy>(() => loadGroupBy());
  const [collapsedSections, setCollapsedSections] = useState<Set<string>>(loadCollapsedSections);
  useEffect(() => persistString(FILTER_KEY, filter), [filter]);
  useEffect(() => persistGroupBy(groupBy), [groupBy]);
  useEffect(() => persistCollapsedSections(collapsedSections), [collapsedSections]);
  const toggleSection = (key: string) =>
    setCollapsedSections((prev) => {
      const next = new Set(prev);
      if (next.has(key)) next.delete(key);
      else next.add(key);
      return next;
    });
  const collapseAll = (sectionKeys: readonly string[]) =>
    setCollapsedSections(new Set(sectionKeys));
  const expandAll = () => setCollapsedSections(new Set());

  const objects = useAllObjectsAcrossInterests();
  const filterLower = filter.trim().toLowerCase();
  const filtered = useMemo(() => {
    if (filterLower.length === 0) return objects;
    return objects.filter(
      (e) =>
        e.objectName.toLowerCase().includes(filterLower) ||
        e.className.toLowerCase().includes(filterLower),
    );
  }, [objects, filterLower]);

  // Tick the inheritance-chain sort when a class resolves, or instances stay sorted by leaf.
  const typeCacheTick = useTypeCacheTick(client);
  const groups = useMemo(
    () => groupEntries(filtered, groupBy, client),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [filtered, groupBy, client, typeCacheTick],
  );
  const inPaneExplorer = useInPaneExplorer();
  const openExplorers = useOpenExplorers();
  const selectedKeys = useMemo(() => {
    const out = new Set<string>();
    if (inPaneExplorer) {
      out.add(pinKey(inPaneExplorer.sessionName, inPaneExplorer.busName, inPaneExplorer.objectName));
    }
    for (const e of openExplorers.values()) {
      out.add(pinKey(e.selection.sessionName, e.selection.busName, e.selection.objectName));
    }
    return out;
  }, [inPaneExplorer, openExplorers]);
  const pinned = usePinnedObjects();
  const hasInterests = objects.length > 0;

  return (
    <div
      style={{
        display: "flex",
        flexDirection: "column",
        height: "100%",
        minHeight: 0,
        gap: "var(--pane-gutter-top)",
      }}
    >
      <Toolbar
        filter={filter}
        onChangeFilter={setFilter}
        groupBy={groupBy}
        onChangeGroupBy={setGroupBy}
        count={filtered.length}
        onCollapseAll={() => collapseAll(groups.map((g) => g.key))}
        onExpandAll={expandAll}
      />
      <div
        style={{
          flex: 1,
          minHeight: 0,
          overflow: "auto",
          padding: "0 2px 16px",
          display: "flex",
          flexDirection: "column",
          gap: 10,
        }}
      >
        {!hasInterests ? (
          <EmptyPane
            heading="No queries declared yet."
            help={[
              <>
                Open the <Mono>Queries</Mono> pane on the left and declare a query on a
                bus. Objects that match will appear here.
              </>,
            ]}
          />
        ) : groups.length === 0 ? (
          <EmptyPane
            heading="No objects match the filter."
            help={[
              <>
                Try clearing the search above, or widen the query on a bus to surface more
                objects.
              </>,
            ]}
          />
        ) : (
          groups.map((g) => (
            <Group
              key={g.key}
              groupKey={g.key}
              label={g.label}
              entries={g.entries}
              client={client}
              showHeader={groupBy !== "flat"}
              groupBy={groupBy}
              collapsed={collapsedSections.has(g.key)}
              onToggleCollapsed={() => toggleSection(g.key)}
              selectedKeys={selectedKeys}
              pinnedKeys={pinned}
              onOpenInPaneExplorer={onOpenInPaneExplorer}
            />
          ))
        )}
      </div>
    </div>
  );
}

function Toolbar({
  filter,
  onChangeFilter,
  groupBy,
  onChangeGroupBy,
  count,
  onCollapseAll,
  onExpandAll,
}: {
  filter: string;
  onChangeFilter: (v: string) => void;
  groupBy: GroupBy;
  onChangeGroupBy: (next: GroupBy) => void;
  count: number;
  onCollapseAll: () => void;
  onExpandAll: () => void;
}) {
  return (
    <div
      style={{
        display: "flex",
        alignItems: "center",
        gap: 10,
        height: "var(--topbar-height)",
        padding: "0 12px",
        borderRadius: "var(--radius-xl)",
        border: "1px solid var(--border-glass)",
        background: "var(--surface-pane-nav)",
        boxShadow: "var(--surface-shadow), var(--surface-glass-bevel)",
        flex: "none",
      }}
    >
      <FilterInput
        value={filter}
        onChange={onChangeFilter}
        placeholder="Filter by name or class..."
        ariaLabel="filter objects"
      />
      <span style={{ flex: 1 }} />
      <span
        style={{
          fontFamily: "var(--font-ui)",
          fontSize: "var(--fs-xs)",
          color: "var(--fg-subtle)",
          letterSpacing: "0.04em",
          textTransform: "uppercase",
        }}
      >
        {count} {count === 1 ? "object" : "objects"}
      </span>
      <SegmentedControl<GroupBy>
        ariaLabel="Group by"
        value={groupBy}
        onChange={onChangeGroupBy}
        options={[
          { value: "bus", label: "Bus", tooltip: "Group by session.bus" },
          { value: "class", label: "Class", tooltip: "Group by class" },
          { value: "query", label: "Query", tooltip: "Group by named query - objects matching multiple queries appear in each" },
          { value: "flat", label: "Flat", tooltip: "No grouping" },
        ]}
      />
      <HoverIconButton
        onClick={onCollapseAll}
        ariaLabel="collapse all sections"
        tooltip="Collapse all"
        icon={<CollapseAllIcon />}
        size={20}
      />
      <HoverIconButton
        onClick={onExpandAll}
        ariaLabel="expand all sections"
        tooltip="Expand all"
        icon={<ExpandAllIcon />}
        size={20}
      />
    </div>
  );
}

function Group({
  groupKey,
  label,
  entries,
  client,
  showHeader,
  groupBy,
  collapsed,
  onToggleCollapsed,
  selectedKeys,
  pinnedKeys,
  onOpenInPaneExplorer,
}: {
  groupKey: string;
  label: string;
  entries: ObjectEntry[];
  client: Client | null;
  showHeader: boolean;
  groupBy: GroupBy;
  collapsed: boolean;
  onToggleCollapsed: () => void;
  selectedKeys: ReadonlySet<string>;
  pinnedKeys: ReadonlyMap<string, { className: string }>;
  onOpenInPaneExplorer: (sel: Selection) => void;
}) {
  // Every row in a bus/query group shares the same session.bus; any entry sources the chip.
  const sample = entries[0];
  let headerLabel: React.ReactNode;
  if (groupBy === "class") {
    headerLabel = <TypeChip client={client} type={label} size="md" />;
  } else if (groupBy === "bus" && sample) {
    headerLabel = <BusChip sessionName={sample.sessionName} busName={sample.busName} size="md" />;
  } else if (groupBy === "query" && sample) {
    headerLabel = (
      <span style={{ display: "inline-flex", alignItems: "center", gap: 6 }}>
        <BusChip sessionName={sample.sessionName} busName={sample.busName} size="md" />
        <span
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            color: "var(--accent-text-wash)",
          }}
        >
          @{sample.queryName}
        </span>
      </span>
    );
  } else {
    headerLabel = (
      <span style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-md)", color: "var(--fg-base)" }}>
        {label}
      </span>
    );
  }
  const body = (
    <AnimatedCardGrid
      groupKey={groupKey}
      entries={entries}
      client={client}
      groupBy={groupBy}
      selectedKeys={selectedKeys}
      pinnedKeys={pinnedKeys}
      onOpenInPaneExplorer={onOpenInPaneExplorer}
    />
  );
  if (!showHeader) {
    return <div style={{ marginBottom: 6 }}>{body}</div>;
  }
  return (
    <CollapsibleSection
      collapsed={collapsed}
      onToggleCollapsed={onToggleCollapsed}
      label={headerLabel}
      count={entries.length}
      ariaLabel={label || groupKey}
    >
      {body}
    </CollapsibleSection>
  );
}

// Opacity enter/leave; resume-grace skips the visibility-restored burst.
function AnimatedCardGrid({
  groupKey,
  entries,
  client,
  groupBy,
  selectedKeys,
  pinnedKeys,
  onOpenInPaneExplorer,
}: {
  groupKey: string;
  entries: ObjectEntry[];
  client: Client | null;
  groupBy: GroupBy;
  selectedKeys: ReadonlySet<string>;
  pinnedKeys: ReadonlyMap<string, { className: string }>;
  onOpenInPaneExplorer: (sel: Selection) => void;
}) {
  // Query mode keys include interestName; other modes are deduped to one entry per object.
  const keyOf = useMemo(() => {
    if (groupBy === "query") {
      return (e: ObjectEntry) =>
        `${e.interestName} ${e.sessionName} ${e.busName} ${e.objectName}`;
    }
    return (e: ObjectEntry) => `${e.sessionName} ${e.busName} ${e.objectName}`;
  }, [groupBy]);
  const { entries: animated, onLeaveComplete } = useAnimatedList(entries, keyOf);
  return (
    <div
      data-section-key={groupKey}
      style={{
        display: "grid",
        gridTemplateColumns: "repeat(auto-fill, minmax(180px, 1fr))",
        gap: 6,
      }}
    >
      {animated.map((a) => {
        const e = a.item;
        const key = pinKey(e.sessionName, e.busName, e.objectName);
        return (
          <AnimatedGridItem
            key={a.key}
            fresh={a.fresh}
            leaving={a.leaving}
            onLeaveComplete={() => onLeaveComplete(a.key)}
          >
            <ObjectTile
              entry={e}
              client={client}
              groupBy={groupBy}
              isSelected={selectedKeys.has(key)}
              isPinned={pinnedKeys.has(key)}
              onClick={() => onOpenInPaneExplorer(toSelection(e))}
            />
          </AnimatedGridItem>
        );
      })}
    </div>
  );
}

function ObjectTile({
  entry,
  client,
  groupBy,
  isSelected,
  isPinned,
  onClick,
}: {
  entry: ObjectEntry;
  client: Client | null;
  groupBy: GroupBy;
  isSelected: boolean;
  isPinned: boolean;
  onClick: () => void;
}) {
  const [hover, setHover] = useState(false);
  // Outline pulse on pin; class flips off via timeout so a re-pin restarts the keyframe.
  const [pinPulse, setPinPulse] = useState(false);
  const pulseTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => {
    return () => {
      if (pulseTimeoutRef.current !== null) clearTimeout(pulseTimeoutRef.current);
    };
  }, []);
  const swatch = useMemo(() => classSwatch(entry.className), [entry.className]);
  const style = objectCardStyle(swatch, hover, isSelected, "tile");
  // Skip chip already shown by the section header.
  const showClassChip = groupBy !== "class";
  const showBusChip = groupBy !== "bus";
  const showPinButton = isPinned || hover;
  // Flat mode: no header, so stack the chips on separate rows.
  const stackChips = groupBy === "flat";
  return (
    <article
      onClick={onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      className={pinPulse ? "pin-pulse" : undefined}
      style={{ ...style, minHeight: 56 }}
    >
      {showPinButton && (
        <PinButton
          pinned={isPinned}
          onClick={(e) => {
            e.stopPropagation();
            const willPin = !isPinned;
            pinnedObjectsActions.toggle(
              pinKey(entry.sessionName, entry.busName, entry.objectName),
              entry.className,
            );
            if (willPin) {
              if (pulseTimeoutRef.current !== null) clearTimeout(pulseTimeoutRef.current);
              setPinPulse(false);
              // Defer one frame so the off->on flip re-triggers the keyframe.
              requestAnimationFrame(() => {
                setPinPulse(true);
                pulseTimeoutRef.current = setTimeout(() => setPinPulse(false), 700);
              });
            }
          }}
        />
      )}
      <span
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-md)",
          fontWeight: 500,
          color: "var(--fg-base)",
          whiteSpace: "nowrap",
          overflow: "hidden",
          textOverflow: "ellipsis",
          minWidth: 0,
        }}
        title={entry.objectName}
      >
        {entry.objectName}
      </span>
      {stackChips ? (
        <span style={{ display: "flex", flexDirection: "column", gap: 4, minWidth: 0 }}>
          {showClassChip && (
            <span style={{ display: "inline-flex", minWidth: 0, overflow: "hidden" }}>
              <TypeChip client={client} type={entry.className} />
            </span>
          )}
          {showBusChip && (
            <span style={{ display: "inline-flex", minWidth: 0, overflow: "hidden" }}>
              <BusChip sessionName={entry.sessionName} busName={entry.busName} />
            </span>
          )}
        </span>
      ) : (
        <span style={{ display: "inline-flex", gap: 4, minWidth: 0, overflow: "hidden", flexWrap: "wrap" }}>
          {showClassChip && <TypeChip client={client} type={entry.className} />}
          {showBusChip && <BusChip sessionName={entry.sessionName} busName={entry.busName} />}
        </span>
      )}
    </article>
  );
}

function PinButton({
  pinned,
  onClick,
}: {
  pinned: boolean;
  onClick: (e: React.MouseEvent<HTMLButtonElement>) => void;
}) {
  const [hover, setHover] = useState(false);
  const color = pinned
    ? "var(--accent-text-wash)"
    : hover
      ? "var(--accent-text-wash)"
      : "var(--fg-subtle)";
  return (
    <button
      type="button"
      onClick={onClick}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      title={pinned ? "Unpin from Cockpit" : "Pin to Cockpit"}
      aria-label={pinned ? "unpin" : "pin"}
      aria-pressed={pinned}
      style={{
        position: "absolute",
        top: 4,
        right: 4,
        width: 18,
        height: 18,
        display: "inline-flex",
        alignItems: "center",
        justifyContent: "center",
        padding: 0,
        background: hover ? "var(--accent-wash)" : "transparent",
        border: "none",
        borderRadius: "var(--radius-sm)",
        cursor: "pointer",
        color,
        transition: "background 0.12s ease, color 0.12s ease",
      }}
    >
      <PinGlyph filled={pinned} />
    </button>
  );
}

function PinGlyph({ filled }: { filled: boolean }) {
  return (
    <svg width="11" height="11" viewBox="0 0 12 12" aria-hidden>
      <path
        d="M5 1 L7 1 L7 5 L9 6.5 L9 7.5 L6.5 7.5 L6.5 10 L6 11 L5.5 10 L5.5 7.5 L3 7.5 L3 6.5 L5 5 Z"
        fill={filled ? "currentColor" : "none"}
        stroke="currentColor"
        strokeLinejoin="round"
        strokeWidth={filled ? 0.8 : 1.2}
      />
    </svg>
  );
}

// -- Data aggregation ------------------------------------------------------------------------

/** One entry per (interest, object). Dedup happens in `groupEntries` for non-query modes. */
interface ObjectEntry {
  sessionName: string;
  busName: string;
  objectName: string;
  className: string;
  /** `${session}.${bus}.${queryName}` */
  interestName: string;
  queryName: string;
}

function useAllObjectsAcrossInterests(): ObjectEntry[] {
  const interests = useAllInterests();
  // useAllInterests fires only on map churn; per-handle membership needs a tick.
  const [tick, setTick] = useState(0);
  useEffect(() => {
    const cancels: Array<() => void> = [];
    for (const [, handle] of interests) {
      cancels.push(handle.onObjectAdded(() => setTick((t) => t + 1)));
      cancels.push(handle.onObjectRemoved(() => setTick((t) => t + 1)));
    }
    return () => {
      for (const c of cancels) c();
    };
  }, [interests]);

  return useMemo(() => {
    const out: ObjectEntry[] = [];
    for (const [interestName, handle] of interests) {
      const parsed = parseInterestName(interestName);
      if (!parsed) continue;
      const objs = handle.objects();
      for (const obj of objs) {
        out.push({
          sessionName: parsed.sessionName,
          busName: parsed.busName,
          objectName: obj.name,
          className: obj.className,
          interestName,
          queryName: parsed.queryName,
        });
      }
    }
    return out;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [interests, tick]);
}

interface GroupedView {
  key: string;
  label: string;
  entries: ObjectEntry[];
}

function groupEntries(
  entries: readonly ObjectEntry[],
  groupBy: GroupBy,
  client: Client | null,
): GroupedView[] {
  // Per-call cache so a 100-card grid doesn't re-walk the type tree per compare.
  const chainCache = new Map<string, string>();
  const chainKey = (cn: string): string => {
    let s = chainCache.get(cn);
    if (s === undefined) {
      s = inheritanceChain(client, cn).join("/");
      chainCache.set(cn, s);
    }
    return s;
  };
  // Primary: inheritance chain so siblings cluster. Tiebreak: object name.
  const compare = (a: ObjectEntry, b: ObjectEntry): number => {
    const byChain = chainKey(a.className).localeCompare(chainKey(b.className));
    if (byChain !== 0) return byChain;
    return a.objectName.localeCompare(b.objectName);
  };

  // Query mode keeps duplicates so multi-query objects appear in every section.
  const dedupedEntries: readonly ObjectEntry[] =
    groupBy === "query" ? entries : dedupByObjectIdentity(entries);

  if (groupBy === "flat") {
    return dedupedEntries.length === 0
      ? []
      : [{ key: "all", label: "", entries: dedupedEntries.slice().sort(compare) }];
  }
  const buckets = new Map<string, ObjectEntry[]>();
  for (const e of dedupedEntries) {
    let k: string;
    if (groupBy === "bus") k = `${e.sessionName}.${e.busName}`;
    else if (groupBy === "class") k = e.className;
    else k = e.interestName;
    let arr = buckets.get(k);
    if (!arr) {
      arr = [];
      buckets.set(k, arr);
    }
    arr.push(e);
  }
  const out: GroupedView[] = [];
  for (const [bucketKey, list] of buckets) {
    list.sort(compare);
    out.push({ key: bucketKey, label: bucketKey, entries: list });
  }
  // Class sections sort by chain; bus/query sort by bucket key.
  if (groupBy === "class") {
    out.sort((a, b) => chainKey(a.label).localeCompare(chainKey(b.label)));
  } else {
    out.sort((a, b) => a.label.localeCompare(b.label));
  }
  return out;
}

function dedupByObjectIdentity(entries: readonly ObjectEntry[]): ObjectEntry[] {
  const seen = new Map<string, ObjectEntry>();
  for (const e of entries) {
    const key = `${e.sessionName} ${e.busName} ${e.objectName}`;
    if (!seen.has(key)) seen.set(key, e);
  }
  return Array.from(seen.values());
}

function toSelection(e: ObjectEntry): Selection {
  return {
    interestName: e.interestName,
    objectName: e.objectName,
    className: e.className,
    sessionName: e.sessionName,
    busName: e.busName,
  };
}

function loadString(key: string, fallback: string): string {
  try {
    return localStorage.getItem(key) ?? fallback;
  } catch {
    return fallback;
  }
}

function persistString(key: string, value: string): void {
  try {
    if (value === "") localStorage.removeItem(key);
    else localStorage.setItem(key, value);
  } catch {
  }
}

function loadGroupBy(): GroupBy {
  const v = loadString(GROUP_BY_KEY, "bus");
  return v === "class" || v === "query" || v === "flat" ? v : "bus";
}

function persistGroupBy(g: GroupBy): void {
  persistString(GROUP_BY_KEY, g);
}

function loadCollapsedSections(): Set<string> {
  try {
    const raw = localStorage.getItem(COLLAPSED_SECTIONS_KEY);
    if (!raw) return new Set();
    const parsed = JSON.parse(raw) as unknown;
    if (!Array.isArray(parsed)) return new Set();
    return new Set(parsed.filter((s): s is string => typeof s === "string"));
  } catch {
    return new Set();
  }
}

function persistCollapsedSections(set: ReadonlySet<string>): void {
  try {
    if (set.size === 0) localStorage.removeItem(COLLAPSED_SECTIONS_KEY);
    else localStorage.setItem(COLLAPSED_SECTIONS_KEY, JSON.stringify(Array.from(set)));
  } catch {
  }
}
