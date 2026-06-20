// === EventRow.tsx ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { memo, useMemo, useState } from "react";

import type { ArgSpec, Client, EventSpec } from "@sen/client";

import { formatTimestamp } from "../core/format.js";
import type { EventDelivery } from "../state/event_store.js";
import { getLastResumeAt } from "../state/visibility.js";
import { ObjectLink } from "../ui/explorer_links.js";
import { ValueRenderer } from "../widgets/value_render/index.js";

// memo'd: deliveries are seq-stable so once a row renders its inputs never change; without
// memo every new event re-renders every existing row.
export const EventRow = memo(EventRowImpl, (prev, next) =>
  prev.delivery === next.delivery &&
  prev.client === next.client &&
  prev.showObject === next.showObject,
);

function EventRowImpl({
  delivery,
  client,
  showObject,
}: {
  delivery: EventDelivery;
  client: Client | null;
  showObject: boolean;
}) {
  // Looked up at render time from the cached class spec so the store doesn't retain it per row.
  const spec = useMemo<EventSpec | undefined>(() => {
    if (!client) return undefined;
    const typeSpec = client.getType(delivery.className);
    if (!typeSpec || typeSpec.data.type !== "sen.kernel.ClassTypeSpec") return undefined;
    return typeSpec.data.value.events.find((e: EventSpec) => e.name === delivery.eventName);
  }, [client, delivery.className, delivery.eventName]);
  // Row-arrival pulse: only deliveries that mount within 1.5s of wire arrival pulse;
  // history/backlog/post-visibility bursts render statically.
  const [isFresh] = useState(() => {
    const now = Date.now();
    return now - delivery.at < 1500 && delivery.at > getLastResumeAt();
  });
  return (
    <div
      className={`events-row${isFresh ? " events-row--arrived" : ""}`}
      style={{
        display: "grid",
        gridTemplateColumns: "94px 1fr",
        gap: 10,
        padding: "5px 12px",
        borderBottom: "1px solid var(--border-default)",
        fontSize: "var(--fs-md)",
      }}
    >
      <span
        style={{ fontFamily: "var(--font-mono)", color: "var(--fg-subtle)", fontSize: "var(--fs-sm)" }}
        title={delivery.timestamp + " UTC (server)"}
      >
        {formatTimestamp(delivery.timestamp)}
      </span>
      <div>
        <div style={{ display: "flex", gap: 8, alignItems: "baseline", flexWrap: "wrap" }}>
          {showObject && (
            <span
              style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-sm)", color: "var(--fg-subtle)" }}
              title={`${delivery.interestName} / ${delivery.className}`}
            >
              <ObjectLink
                selection={{
                  interestName: delivery.interestName,
                  objectName: delivery.objectName,
                  className: delivery.className,
                  sessionName: delivery.sessionName,
                  busName: delivery.busName,
                }}
              />
            </span>
          )}
          <span style={{ fontFamily: "var(--font-mono)", color: "var(--fg-base)" }}>
            {delivery.eventName}
          </span>
        </div>
        {spec && spec.args.length > 0 && (
          <div
            style={{
              marginTop: 2,
              display: "flex",
              gap: 12,
              flexWrap: "wrap",
              alignItems: "baseline",
              color: "var(--fg-muted)",
              fontSize: "var(--fs-sm)",
              paddingLeft: 4,
            }}
          >
            {spec.args.map((arg: ArgSpec, i: number) => (
              <span
                key={arg.name}
                style={{ display: "inline-flex", gap: 4, alignItems: "baseline", flexWrap: "wrap" }}
              >
                <span style={{ fontFamily: "var(--font-mono)", color: "var(--fg-subtle)" }}>
                  {arg.name}:
                </span>
                <ValueRenderer client={client} declaredType={arg.type} value={delivery.args[i]} />
              </span>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
