// === GroupHeader.tsx =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { Caret } from "../widgets/interests/icons.js";

// Headers are ordered base-class -> leaf, matching the STL `extends` reading direction.
export function GroupHeader({
  className,
  collapsed,
  onToggle,
}: {
  className: string;
  collapsed: boolean;
  onToggle: () => void;
}) {
  return (
    <div
      onClick={onToggle}
      role="button"
      tabIndex={0}
      aria-expanded={!collapsed}
      style={{
        cursor: "pointer",
        display: "flex",
        alignItems: "center",
        gap: 6,
        padding: "6px 12px 6px 0",
        fontSize: "var(--fs-md)",
        fontWeight: 500,
        color: "var(--fg-base)",
        fontFamily: "var(--font-mono)",
      }}
    >
      <Caret open={!collapsed} />
      {className}
    </div>
  );
}
