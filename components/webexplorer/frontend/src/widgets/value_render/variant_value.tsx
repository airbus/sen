// === VariantValue.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useContext } from "react";

import type { Client, Var, Variant } from "@sen/client";

import { isComposite } from "../../core/types.js";
import { StableHeight } from "../../ui/layout.js";
import { TypeChip } from "../../ui/chips.js";
import { ValueRenderer } from "./value_renderer.js";
import type { RenderLeafActions } from "./leaf_actions.js";
import { ValueFilterContext } from "./value_row.js";

// StableHeight pins the cell so arm-switches don't bump siblings.
export function VariantValue({
  client,
  v,
  path,
  renderLeafActions,
}: {
  client: Client | null;
  v: Variant;
  path: string;
  renderLeafActions?: RenderLeafActions | undefined;
}) {
  const armType = v.type;
  const payload = v.value as Var;
  const payloadIsBlock = isComposite(client, armType);
  // Same own-match-clears-subtree rule as struct fields.
  const filter = useContext(ValueFilterContext);
  const ownMatch = filter ? armType.toLowerCase().includes(filter) : false;
  const subFilter = filter && !ownMatch ? filter : "";
  return (
    <StableHeight>
      <span
        style={{
          display: "flex",
          flex: 1,
          flexDirection: payloadIsBlock ? "column" : "row",
          alignItems: payloadIsBlock ? "flex-start" : "center",
          gap: payloadIsBlock ? 2 : 6,
          flexWrap: payloadIsBlock ? "nowrap" : "wrap",
          minWidth: 0,
        }}
      >
        <TypeChip client={client} type={armType} />
        {/* align-self stretch on the payload alone so its grid fills the row width. */}
        <ValueFilterContext.Provider value={subFilter}>
          {payloadIsBlock ? (
            <div style={{ alignSelf: "stretch", minWidth: 0, width: "100%" }}>
              <ValueRenderer
                client={client}
                declaredType={armType}
                value={payload}
                path={path}
                renderLeafActions={renderLeafActions}
              />
            </div>
          ) : (
            <ValueRenderer
              client={client}
              declaredType={armType}
              value={payload}
              path={path}
              renderLeafActions={renderLeafActions}
            />
          )}
        </ValueFilterContext.Provider>
      </span>
    </StableHeight>
  );
}
