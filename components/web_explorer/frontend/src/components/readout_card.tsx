// === ReadoutCard.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";
import { useMemo } from "react";

import type { Client, Var } from "@sen/client";
import { Variant } from "@sen/client";
import { useObject } from "@sen/client/react";

import { isComposite, kindOf } from "../core/types.js";
import {
  makePlotKeyFromSource,
  makeWatchKey,
  type WatchSource,
} from "../core/watch_keys.js";
import { useInterestByName } from "../state/interest_registry.js";
import { PropertyKindIconCell } from "./property_kind_icon_cell.js";
import { useLiveProperty } from "../state/live_value.js";
import { useIsPlotted, watchActions } from "../state/watch_plot.js";
import { PropertyLink } from "../ui/explorer_links.js";
import { HoverIconButton } from "../ui/buttons.js";
import { LeftTruncated, OfflineText } from "../ui/text_primitives.js";

import {
  ChevronDownIcon,
  ChevronRightIcon,
  CloseIcon,
  ICON_BUTTON_SIZE,
} from "../ui/icons.js";
import { StableHeight } from "../ui/layout.js";
import { TypeChip } from "../ui/chips.js";
import {
  buildLeafActionsRenderer,
  PlotToggleButton,
  rootPlotKind,
  ValueRenderer,
} from "../widgets/value_render/index.js";
import { useFlash } from "../widgets/value_render/value_row.js";
import { useRef } from "react";

export function ReadoutCard({
  client,
  source,
  modeOverridden,
  onToggleModeOverride,
}: {
  client: Client | null;
  source: WatchSource;
  modeOverridden: boolean;
  onToggleModeOverride: () => void;
}) {
  const interest = useInterestByName(source.interestName);
  const obj = useObject(interest, source.objectName);
  const live = useLiveProperty(obj ?? null, source.propertyName);
  const key = makeWatchKey(source);
  const ok = !!obj;
  // Per-key plotted selector so unrelated toggles don't churn every Watches card.
  const rootPlotKey = useMemo(() => makePlotKeyFromSource(source, ""), [source]);
  const rootPlotted = useIsPlotted(rootPlotKey);
  // Card shape:
  //  - Scalars: no chevron, inline value + meta strip
  //  - Variants: chevron, default compact (arm chip only); payload tree on expand
  //  - Other composites: chevron, default expanded (value IS the tree)
  const composite = isComposite(client, source.declaredType);
  const isVariant = kindOf(client, source.declaredType) === "variant";
  const scalarFixed = !composite;
  const defaultCompact = !composite || isVariant;
  const compact = scalarFixed ? false : modeOverridden ? !defaultCompact : defaultCompact;
  // Identity-stable per (source, client) so React.memo at LeafActions can bail across ticks.
  const renderLeafActionsExpanded = useMemo(
    () =>
      buildLeafActionsRenderer({
        source,
        onTogglePlottedLeaf: watchActions.togglePlottedLeaf,
        client,
      }),
    [source, client],
  );
  const renderLeafActions = compact ? undefined : renderLeafActionsExpanded;
  const rootKind = rootPlotKind(live.value, source.declaredType, client);
  const showHeaderPlot = rootKind !== null;
  return (
    <div
      style={{
        background: "var(--watch-level-2)",
        border: "1px solid var(--border-glass)",
        borderRadius: "var(--radius-lg)",
        boxShadow: "inset 0 1px 0 var(--surface-glass-highlight)",
        padding: scalarFixed
          ? "2px 6px 2px 8px"
          : compact
            ? "3px 6px 3px 4px"
            : "5px 6px 5px 8px",
        display: "flex",
        flexDirection: "column",
        gap: scalarFixed ? 0 : compact ? 0 : 4,
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
        {!scalarFixed && (
          <HoverIconButton
            onClick={onToggleModeOverride}
            ariaLabel={compact ? "expand card" : "compact card"}
            tooltip={compact ? "Expand" : "Compact"}
            icon={compact ? <ChevronRightIcon /> : <ChevronDownIcon />}
            size={16}
          />
        )}
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
            // Cap so LeftTruncated takes over and trims from the start past the limit.
            maxWidth: "60%",
            display: "block",
            overflow: "hidden",
          }}
        >
          <LeftTruncated text={source.propertyName} />
        </PropertyLink>
        {(!composite || isVariant) && (
          <InlineValue
            client={client}
            declaredType={source.declaredType}
            value={live.value}
            ok={ok}
            truncate={compact}
            variantArmOnly={isVariant}
          />
        )}
        <span
          style={{
            marginLeft: "auto",
            display: "inline-flex",
            alignItems: "center",
            gap: 5,
            flex: "none",
          }}
        >
          {composite && !isVariant && !compact && !ok && (
            <span
              style={{
                fontFamily: "var(--font-mono)",
                fontSize: "var(--fs-xs)",
                color: "var(--fg-subtle)",
                textTransform: "uppercase",
                letterSpacing: "0.05em",
              }}
            >
              offline
            </span>
          )}
          {showHeaderPlot && (
            <PlotToggleButton
              plotted={rootPlotted}
              onClick={() => watchActions.togglePlottedLeaf(source, "", rootKind!)}
            />
          )}
          <HoverIconButton
            onClick={() => watchActions.removeWatch(key)}
            ariaLabel={`remove ${source.objectName}.${source.propertyName}`}
            tooltip="Remove from Watch"
            icon={<CloseIcon />}
            danger
          />
        </span>
      </div>
      {!compact && composite && ok && live.value !== undefined && (
        <div style={{ width: "100%", minWidth: 0, overflow: "hidden" }}>
          {isVariant && live.value instanceof Variant ? (
            <StableHeight>
              <ValueRenderer
                client={client}
                declaredType={live.value.type}
                value={live.value.value as Var}
                renderLeafActions={renderLeafActions}
              />
            </StableHeight>
          ) : (
            <ValueRenderer
              client={client}
              declaredType={source.declaredType}
              value={live.value}
              renderLeafActions={renderLeafActions}
            />
          )}
        </div>
      )}
      {!compact && composite && !isVariant && ok && live.value === undefined && (
        <span style={{ fontFamily: "var(--font-mono)", color: "var(--fg-subtle)", fontSize: "var(--fs-md)" }}>
          --
        </span>
      )}
    </div>
  );
}

function InlineValue({
  client,
  declaredType,
  value,
  ok,
  truncate,
  variantArmOnly,
}: {
  client: Client | null;
  declaredType: string;
  value: Var | undefined;
  ok: boolean;
  truncate: boolean;
  /** When true and value is a Variant, render only the arm chip; payload renders on expand. */
  variantArmOnly?: boolean;
}) {
  const fontStyle: React.CSSProperties = {
    fontFamily: "var(--font-mono)",
    fontSize: "var(--fs-md)",
    color: "var(--fg-base)",
    textAlign: "right",
  };
  // ReadoutCard has no .property-row ancestor; scheduler tints .value-text via [data-flash].
  const hostRef = useRef<HTMLSpanElement | null>(null);
  useFlash(value, undefined, hostRef);
  const valueTextClass = "value-text";
  const renderInner = () => {
    if (!ok) return <OfflineText />;
    if (value === undefined) return <MutedInlineText>--</MutedInlineText>;
    if (variantArmOnly && value instanceof Variant) {
      return <TypeChip client={client} type={value.type} />;
    }
    return <ValueRenderer client={client} declaredType={declaredType} value={value} />;
  };
  if (truncate) {
    return (
      <span
        ref={hostRef}
        className={valueTextClass}
        style={{
          flex: 1,
          minWidth: 0,
          paddingLeft: 6,
          overflow: "hidden",
          whiteSpace: "nowrap",
          textOverflow: "ellipsis",
          ...fontStyle,
        }}
      >
        {renderInner()}
      </span>
    );
  }
  return (
    <span
      ref={hostRef}
      className={valueTextClass}
      style={{
        flex: 1,
        minWidth: 0,
        paddingLeft: 6,
        display: "flex",
        justifyContent: "flex-end",
        alignItems: "baseline",
        gap: 4,
        flexWrap: "wrap",
        wordBreak: "break-word",
        overflow: "hidden",
        ...fontStyle,
      }}
    >
      {renderInner()}
    </span>
  );
}

function MutedInlineText({ children }: { children: React.ReactNode }) {
  return (
    <span style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-md)", color: "var(--fg-subtle)" }}>
      {children}
    </span>
  );
}
