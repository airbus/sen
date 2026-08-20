// === StructValue.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useContext, useMemo } from "react";

import type { Client, Var } from "@sen/client";

import { isComposite } from "../../core/types.js";
import { collectTypeSearchTerms } from "../../core/value_walk.js";
import { HelpButton } from "../../ui/buttons.js";
import { Muted } from "../../ui/text_primitives.js";
import { Tooltip } from "../../ui/tooltip.js";
import { ValueRenderer } from "./value_renderer.js";
import type { RenderLeafActions } from "./leaf_actions.js";
import { ValueFilterContext, ValueRow } from "./value_row.js";

export function StructValue({
  client,
  value,
  declaredType,
  path,
  renderLeafActions,
}: {
  client: Client | null;
  value: Record<string, Var>;
  declaredType: string;
  path: string;
  renderLeafActions?: RenderLeafActions | undefined;
}) {
  const { fieldTypes, fieldDescriptions, specFieldOrder } = useMemo<{
    fieldTypes: Record<string, string>;
    fieldDescriptions: Record<string, string>;
    specFieldOrder: string[] | null;
  }>(() => {
    const spec = client?.getType(declaredType);
    if (!spec || spec.data.type !== "sen.kernel.StructTypeSpec") {
      return { fieldTypes: {}, fieldDescriptions: {}, specFieldOrder: null };
    }
    const fields = spec.data.value.fields;
    const types: Record<string, string> = {};
    const descriptions: Record<string, string> = {};
    const order: string[] = [];
    for (const f of fields) {
      types[f.name] = f.type;
      descriptions[f.name] = f.description;
      order.push(f.name);
    }
    return { fieldTypes: types, fieldDescriptions: descriptions, specFieldOrder: order };
  }, [client, declaredType]);
  // Memoed so a fresh array reference each tick doesn't bust downstream deps.
  const fieldOrder = useMemo(
    () => (specFieldOrder ? specFieldOrder.filter((n) => n in value) : Object.keys(value)),
    [specFieldOrder, value],
  );

  // Empty filter means show all; ancestor matches clear it so the subtree shows fully.
  const filter = useContext(ValueFilterContext);
  const visibleFieldOrder = useMemo(() => {
    if (!filter) return fieldOrder;
    return fieldOrder.filter((name) =>
      fieldMatchesFilter(client, name, fieldTypes[name] ?? "unknown", filter),
    );
  }, [fieldOrder, fieldTypes, client, filter]);

  if (fieldOrder.length === 0) {
    return <Muted>{"{}"}</Muted>;
  }
  if (visibleFieldOrder.length === 0) {
    // Collapse so the parent's empty-state can take over.
    return null;
  }

  return (
    <div
      style={{
        marginLeft: 2,
        paddingLeft: 8,
        borderLeft: "1px solid var(--border-default)",
        display: "grid",
        // name | value | actions; fixed actions column so every row's icons align in x.
        gridTemplateColumns: `auto minmax(0, 1fr) ${ACTIONS_COLUMN_WIDTH}px`,
        rowGap: 2,
        alignItems: "baseline",
      }}
    >
      {visibleFieldOrder.map((name) => {
        const fieldPath = path ? `${path}.${name}` : name;
        const fieldType = fieldTypes[name] ?? "unknown";
        const v = value[name] as Var;
        const composite = isComposite(client, fieldType);
        // Own-name match clears the subFilter so the whole subtree shows.
        const ownMatch = filter ? name.toLowerCase().includes(filter) : false;
        const subFilter = filter && !ownMatch ? filter : "";
        // Composites don't flash; their primitive leaves do via flashKey={v}.
        return (
          <ValueRow key={name} flashKey={composite ? undefined : v}>
            {composite ? (
              <>
                <Tooltip content={fieldDescriptions[name]}>
                  <span style={fieldNameStyle}>{name}</span>
                </Tooltip>
                <span aria-hidden />
                {(renderLeafActions || fieldDescriptions[name]) && (
                  <span
                    style={{
                      display: "inline-flex",
                      alignItems: "center",
                      justifySelf: "end",
                      gap: 4,
                    }}
                  >
                    <HelpButton
                      description={fieldDescriptions[name]}
                      ariaLabel={`${name} documentation`}
                    />
                    {renderLeafActions && renderLeafActions(fieldPath, v, fieldType)}
                  </span>
                )}
                {/* Span all three so the nested actions column aligns with the outer. */}
                <div style={{ gridColumn: "1 / -1", marginTop: 2 }}>
                  <ValueFilterContext.Provider value={subFilter}>
                    <ValueRenderer
                      client={client}
                      declaredType={fieldType}
                      value={v}
                      path={fieldPath}
                      renderLeafActions={renderLeafActions}
                    />
                  </ValueFilterContext.Provider>
                </div>
              </>
            ) : (
              <>
                <Tooltip content={fieldDescriptions[name]}>
                  <span style={fieldNameStyle}>{name}</span>
                </Tooltip>
                <span
                  className="value-cell"
                  style={{ display: "inline-flex", minWidth: 0, alignItems: "center" }}
                >
                  <span className="value-text">
                    <ValueRenderer
                      client={client}
                      declaredType={fieldType}
                      value={v}
                      path={fieldPath}
                      renderLeafActions={renderLeafActions}
                    />
                  </span>
                </span>
                {(renderLeafActions || fieldDescriptions[name]) && (
                  <span
                    style={{
                      display: "inline-flex",
                      alignItems: "center",
                      justifySelf: "end",
                      gap: 4,
                    }}
                  >
                    <HelpButton
                      description={fieldDescriptions[name]}
                      ariaLabel={`${name} documentation`}
                    />
                    {renderLeafActions && renderLeafActions(fieldPath, v, fieldType)}
                  </span>
                )}
              </>
            )}
          </ValueRow>
        );
      })}
    </div>
  );
}

// Width fits three 24x24 IconToggles + 4px gaps; mirrored by property_row.
export const ACTIONS_COLUMN_WIDTH = 108;

// Invalidated on onTypeAdded so a class whose spec arrives mid-session isn't stuck negative.
const NULL_CLIENT_BLOBS = new Map<string, string>();
const blobCacheByClient = new WeakMap<Client, Map<string, string>>();
const wiredTypeInvalidation = new WeakSet<Client>();

function lowerSearchBlob(client: Client | null, fieldType: string): string {
  const cache = client ? getOrInitCache(client) : NULL_CLIENT_BLOBS;
  const cached = cache.get(fieldType);
  if (cached !== undefined) return cached;
  const terms: string[] = [];
  collectTypeSearchTerms(client, fieldType, terms);
  const blob = terms.length === 0 ? "" : terms.join("\0").toLowerCase();
  cache.set(fieldType, blob);
  return blob;
}

function getOrInitCache(client: Client): Map<string, string> {
  let cache = blobCacheByClient.get(client);
  if (cache === undefined) {
    cache = new Map();
    blobCacheByClient.set(client, cache);
  }
  if (!wiredTypeInvalidation.has(client)) {
    wiredTypeInvalidation.add(client);
    // Client-lifetime subscription; the handler GCs with the closed Client.
    client.onTypeAdded(() => {
      blobCacheByClient.get(client)?.clear();
    });
  }
  return cache;
}

// Filter is assumed lowercase. Pass on own-name match or any descendant type term.
export function fieldMatchesFilter(
  client: Client | null,
  fieldName: string,
  fieldType: string,
  filter: string,
): boolean {
  if (!filter) return true;
  if (fieldName.toLowerCase().includes(filter)) return true;
  return lowerSearchBlob(client, fieldType).includes(filter);
}

const fieldNameStyle = {
  fontFamily: "var(--font-mono)",
  fontSize: "var(--fs-md)",
  color: "var(--fg-muted)",
  whiteSpace: "nowrap" as const,
  overflow: "hidden" as const,
  textOverflow: "ellipsis" as const,
  maxWidth: 200,
};
