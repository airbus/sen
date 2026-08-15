// === MethodTable.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useMemo, useState } from "react";

import type { ArgSpec, Client, ObjectHandle } from "@sen/client";

import { type ClassMemberGroup } from "../state/class_members.js";
import { Collapsible } from "../ui/layout.js";
import { EmptyHint } from "../ui/empty_state.js";
import { MethodInvoker } from "../widgets/value_edit/index.js";
import { GroupHeader } from "./group_header.js";

export function MethodTable({
  client,
  obj,
  groups,
  filter,
}: {
  client: Client;
  obj: ObjectHandle;
  groups: ClassMemberGroup[];
  filter: string;
}) {
  const filterLower = filter.trim().toLowerCase();
  // Drop property-setter/getter shadows (reachable via the property writer) then apply the
  // text filter; empty groups dropped so headerless classes don't appear.
  const surfaceGroups = useMemo(
    () =>
      [...groups]
        .reverse()
        .map((g) => ({
          ...g,
          methods: g.methods
            .filter((m) => m.propertyRelation === "nonPropertyRelated")
            .filter((m) => {
              if (!filterLower) return true;
              if (m.name.toLowerCase().includes(filterLower)) return true;
              for (const a of m.args) {
                if (a.name.toLowerCase().includes(filterLower)) return true;
                if (a.type.toLowerCase().includes(filterLower)) return true;
              }
              if (m.returnType && m.returnType.toLowerCase().includes(filterLower)) return true;
              return false;
            }),
        }))
        .filter((g) => g.methods.length > 0),
    [groups, filterLower],
  );
  const [collapsedClasses, setCollapsedClasses] = useState<ReadonlySet<string>>(new Set());
  const toggleClassCollapsed = (className: string) =>
    setCollapsedClasses((prev) => {
      const next = new Set(prev);
      if (next.has(className)) next.delete(className);
      else next.add(className);
      return next;
    });
  // Accordion: only one method expanded at a time; key includes className so inherited
  // methods with the same name don't collide.
  const [openMethodKey, setOpenMethodKey] = useState<string | null>(null);
  if (surfaceGroups.length === 0) {
    return (
      <div>
        <EmptyHint>{filterLower ? "no methods match the filter." : "no methods"}</EmptyHint>
      </div>
    );
  }
  const showGroupHeaders = surfaceGroups.length > 1;
  return (
    <div>
      {surfaceGroups.map((g) => {
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
                onToggle={() => toggleClassCollapsed(g.className)}
              />
            )}
            <Collapsible open={!classCollapsed}>
              {g.methods.map((m) => {
                const methodKey = `${g.className}.${m.name}`;
                return (
                  <MethodInvoker
                    key={m.name}
                    client={client}
                    obj={obj}
                    method={{
                      name: m.name,
                      description: m.description,
                      args: m.args.map((a: ArgSpec) => ({
                        name: a.name,
                        description: a.description,
                        type: a.type,
                      })),
                      returnType: m.returnType,
                    }}
                    expanded={openMethodKey === methodKey}
                    onExpandedChange={(next) =>
                      setOpenMethodKey(next ? methodKey : null)
                    }
                  />
                );
              })}
            </Collapsible>
          </div>
        );
      })}
    </div>
  );
}
