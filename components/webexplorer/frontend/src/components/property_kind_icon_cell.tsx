// === PropertyKindIconCell.tsx ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type { Client } from "@sen/client";

import {
  isDynamicCategory,
  isWritableCategory,
  propertyCategoryOf,
} from "../state/class_members.js";
import { PropertyKindIcon } from "../ui/icons.js";

export function PropertyKindIconCell({
  client,
  className,
  propertyName,
}: {
  client: Client | null;
  className: string;
  propertyName: string;
}) {
  const category = propertyCategoryOf(client, className, propertyName);
  return (
    <span
      style={{ display: "inline-flex", color: "var(--fg-subtle)", flex: "none" }}
      title={`property - ${category ?? "unknown category"}`}
    >
      <PropertyKindIcon
        dynamic={isDynamicCategory(category)}
        writable={isWritableCategory(category)}
      />
    </span>
  );
}
