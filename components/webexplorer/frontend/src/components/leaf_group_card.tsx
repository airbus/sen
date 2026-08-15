// === LeafGroupCard.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo, useRef } from "react";

import type { Client, Var } from "@sen/client";
import { useObject } from "@sen/client/react";

import { extractAt, typeAtPath } from "../core/value_walk.js";
import {
  makePlotKeyFromSource,
  makeWatchKey,
  type WatchSource,
} from "../core/watch_keys.js";
import { useInterestByName } from "../state/interest_registry.js";
import { useLiveProperty } from "../state/live_value.js";
import { useIsPlotted, watchActions } from "../state/watch_plot.js";
import { PropertyLink } from "../ui/explorer_links.js";
import { HoverIconButton } from "../ui/buttons.js";
import { LeftTruncated, OfflineText } from "../ui/text_primitives.js";
import { CloseIcon, ICON_BUTTON_SIZE } from "../ui/icons.js";
import {
  PlotToggleButton,
  rootPlotKind,
  ValueRenderer,
} from "../widgets/value_render/index.js";
import { useFlash } from "../widgets/value_render/value_row.js";
import { PropertyKindIconCell } from "./property_kind_icon_cell.js";

// Subscribes the property once; each row slices via extractAt.
export function LeafGroupCard({
  client,
  source,
  leafPaths,
}: {
  client: Client | null;
  source: WatchSource;
  leafPaths: readonly string[];
}) {
  const interest = useInterestByName(source.interestName);
  const obj = useObject(interest, source.objectName);
  const live = useLiveProperty(obj ?? null, source.propertyName);
  const ok = !!obj;
  return (
    <div
      style={{
        background: "var(--watch-level-2)",
        border: "1px solid var(--border-glass)",
        borderRadius: "var(--radius-lg)",
        boxShadow: "inset 0 1px 0 var(--surface-glass-highlight)",
        padding: "5px 6px 5px 8px",
        display: "flex",
        flexDirection: "column",
        gap: 4,
        alignItems: "stretch",
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
        <PropertyKindIconCell
          client={client}
          className={source.className}
          propertyName={source.propertyName}
        />
        <PropertyLink
          selection={{
            interestName: source.interestName,
            objectName: source.objectName,
            className: source.className,
            sessionName: source.sessionName,
            busName: source.busName,
          }}
          propertyName={source.propertyName}
          title={`${source.objectName}.${source.propertyName} on ${source.interestName}`}
          style={{
            fontFamily: "var(--font-mono)",
            fontSize: "var(--fs-md)",
            fontWeight: 500,
            color: "var(--fg-base)",
            flex: "1 1 auto",
            minWidth: 0,
            display: "block",
            overflow: "hidden",
          }}
        >
          <LeftTruncated text={source.propertyName} />
        </PropertyLink>
        <span style={{ marginLeft: "auto", flex: "none" }}>
          <HoverIconButton
            onClick={() => {
              // Single batchEdit; N sequential removes would N^2 the selector fan-out.
              watchActions.batchEdit(
                leafPaths.map((p) => makeWatchKey({ ...source, leafPath: p })),
                [],
              );
            }}
            ariaLabel={`stop watching all fields of ${source.propertyName}`}
            tooltip="Stop watching this property"
            icon={<CloseIcon />}
            danger
          />
        </span>
      </div>
      {leafPaths.map((p) => {
        const leafKey = makeWatchKey({ ...source, leafPath: p });
        const leafValue = ok ? extractAt(live.value, p) : undefined;
        // Walk the schema to the leaf's own type; the parent struct type would mis-format
        // ints as floats, lose enum chips, and skip TimeStamp formatting.
        const leafType = typeAtPath(client, source.declaredType, p) ?? source.declaredType;
        const plotKind = rootPlotKind(leafValue, leafType, client);
        return (
          <LeafRow
            key={p}
            client={client}
            source={source}
            leafPath={p}
            declaredType={leafType}
            leafValue={leafValue}
            ok={ok}
            label={p}
            plotKind={plotKind}
            onTogglePlot={() => watchActions.togglePlottedLeaf(source, p, plotKind!)}
            onRemove={() => watchActions.removeWatch(leafKey)}
            ariaRemove={`stop watching ${source.propertyName}.${p}`}
          />
        );
      })}
    </div>
  );
}

// Extracted so each row holds its own useFlash + useIsPlotted; without per-row scoping any
// plot toggle would re-render every leaf row across every card.
function LeafRow({
  client,
  source,
  leafPath,
  declaredType,
  leafValue,
  ok,
  label,
  plotKind,
  onTogglePlot,
  onRemove,
  ariaRemove,
}: {
  client: Client | null;
  source: WatchSource;
  leafPath: string;
  declaredType: string;
  leafValue: Var | undefined;
  ok: boolean;
  label: string;
  plotKind: ReturnType<typeof rootPlotKind>;
  onTogglePlot: () => void;
  onRemove: () => void;
  ariaRemove: string;
}) {
  const plotKey = useMemo(() => makePlotKeyFromSource(source, leafPath), [source, leafPath]);
  const isPlotted = useIsPlotted(plotKey);
  const plotted = plotKind ? isPlotted : false;
  // useFlash is a no-op outside FlashEnabledContext; scheduler tints via [data-flash].
  const hostRef = useRef<HTMLSpanElement | null>(null);
  useFlash(leafValue, undefined, hostRef);
  const valueTextClass = "value-text";
  return (
    <div
      style={{
        // Grid (not flex): label column width must be content-independent or LeftTruncated's
        // scrollWidth vs clientWidth check would oscillate as the basis followed the text.
        display: "grid",
        gridTemplateColumns: "minmax(0, 1fr) auto auto auto",
        alignItems: "center",
        columnGap: 8,
        padding: "4px 6px",
        marginTop: 4,
        background: "var(--watch-level-3)",
        borderRadius: "var(--radius-sm)",
      }}
    >
      <LeftTruncated
        text={label}
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-sm)",
          color: "var(--fg-muted)",
        }}
      />
      <span
        ref={hostRef}
        className={valueTextClass}
        style={{
          minWidth: 0,
          textAlign: "right",
          whiteSpace: "nowrap",
        }}
      >
        {ok ? (
          <ValueRenderer client={client} declaredType={declaredType} value={leafValue} />
        ) : (
          <OfflineText />
        )}
      </span>
      {plotKind ? (
        <PlotToggleButton plotted={plotted} onClick={onTogglePlot} />
      ) : (
        <span />
      )}
      <HoverIconButton
        onClick={onRemove}
        ariaLabel={ariaRemove}
        tooltip="Stop watching this field"
        icon={<CloseIcon />}
        danger
      />
    </div>
  );
}
