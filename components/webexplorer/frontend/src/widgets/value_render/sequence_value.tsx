// === sequence_value.tsx ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type * as React from "react";
import { useMemo, useState } from "react";

import type { Client, Var } from "@sen/client";

import { isComposite } from "../../core/types.js";
import { Muted } from "../../ui/text_primitives.js";
import { ValueRenderer } from "./value_renderer.js";
import type { RenderLeafActions } from "./leaf_actions.js";
import { ACTIONS_COLUMN_WIDTH } from "./struct_value.js";
import { ValueRow } from "./value_row.js";

export function SequenceValue({
  client,
  value,
  declaredType,
  path,
  renderLeafActions,
}: {
  client: Client | null;
  value: Var[];
  declaredType: string;
  path: string;
  renderLeafActions?: RenderLeafActions | undefined;
}) {
  // null = no override; default expanded when <= 6 so growth/shrink re-applies the default.
  const [userExpanded, setUserExpanded] = useState<boolean | null>(null);
  const expanded = userExpanded ?? value.length <= 6;
  const setExpanded: React.Dispatch<React.SetStateAction<boolean>> = (next) => {
    setUserExpanded((prev) => {
      const current = prev ?? value.length <= 6;
      return typeof next === "function" ? next(current) : next;
    });
  };

  const elementType = useMemo(() => {
    const spec = client?.getType(declaredType);
    if (spec && spec.data.type === "sen.kernel.SequenceTypeSpec") {
      return spec.data.value.elementType;
    }
    return "unknown";
  }, [client, declaredType]);

  if (value.length === 0) {
    return <Muted>[]</Muted>;
  }

  return (
    <div>
      <button
        type="button"
        onClick={() => setExpanded((e) => !e)}
        style={{
          fontFamily: "var(--font-mono)",
          fontSize: "var(--fs-sm)",
          color: "var(--fg-muted)",
          background: "transparent",
          border: "1px solid var(--border-default)",
          borderRadius: "var(--radius-sm)",
          padding: "1px 6px",
          cursor: "pointer",
        }}
      >
        {expanded ? "▾" : "▸"} {value.length} item{value.length === 1 ? "" : "s"}
      </button>
      {expanded && (
        <div
          style={{
            marginTop: 4,
            marginLeft: 8,
            display: "flex",
            flexDirection: "column",
            gap: 2,
          }}
        >
          {value.map((v, i) => {
            const elemPath = `${path}[${i}]`;
            const composite = isComposite(client, elementType);
            const indexLabel = (
              <span
                style={{
                  fontFamily: "var(--font-mono)",
                  fontSize: "var(--fs-sm)",
                  color: "var(--fg-subtle)",
                  minWidth: 18,
                }}
              >
                [{i}]
              </span>
            );
            return (
              <ValueRow key={i} flashKey={composite ? undefined : v}>
                {composite ? (
                  <>
                    <span style={{ display: "inline-flex", flex: "1 1 auto", minWidth: 0 }}>
                      {indexLabel}
                    </span>
                    {renderLeafActions && (
                      <span
                        style={{
                          display: "inline-flex",
                          alignItems: "center",
                          justifyContent: "flex-end",
                          marginLeft: "auto",
                          gap: 4,
                          width: ACTIONS_COLUMN_WIDTH,
                        }}
                      >
                        {renderLeafActions(elemPath, v, elementType)}
                      </span>
                    )}
                    <div style={{ width: "100%", marginTop: 2 }}>
                      <ValueRenderer
                        client={client}
                        declaredType={elementType}
                        value={v}
                        path={elemPath}
                        renderLeafActions={renderLeafActions}
                      />
                    </div>
                  </>
                ) : (
                  <>
                    <span style={{ display: "inline-flex", flex: "0 1 auto", minWidth: 0 }}>
                      {indexLabel}
                    </span>
                    <span
                      className="value-cell"
                      style={{ display: "inline-flex", minWidth: 60, alignItems: "center", gap: 8, flex: "1 1 auto" }}
                    >
                      <span className="value-text">
                        <ValueRenderer
                          client={client}
                          declaredType={elementType}
                          value={v}
                          path={elemPath}
                          renderLeafActions={renderLeafActions}
                        />
                      </span>
                    </span>
                    {renderLeafActions && (
                      <span
                        style={{
                          display: "inline-flex",
                          alignItems: "center",
                          justifyContent: "flex-end",
                          marginLeft: "auto",
                          gap: 4,
                          width: ACTIONS_COLUMN_WIDTH,
                        }}
                      >
                        {renderLeafActions(elemPath, v, elementType)}
                      </span>
                    )}
                  </>
                )}
              </ValueRow>
            );
          })}
        </div>
      )}
    </div>
  );
}
