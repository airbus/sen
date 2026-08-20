// === AdaptivePicker.tsx ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { PopupPicker } from "./popup_picker.js";
import { SegmentedPicker } from "./segmented_picker.js";

export interface PickerItem {
  name: string;
  description: string;
  label?: string;
}

export function AdaptivePicker({
  items,
  value,
  onChange,
}: {
  items: readonly PickerItem[];
  value: string;
  onChange: (v: string) => void;
}) {
  const SEGMENTED_MAX = 4;
  const SEARCH_MIN = 13;
  if (items.length <= SEGMENTED_MAX) {
    return <SegmentedPicker items={items} value={value} onChange={onChange} />;
  }
  return (
    <PopupPicker
      items={items}
      value={value}
      onChange={onChange}
      searchable={items.length >= SEARCH_MIN}
    />
  );
}
