// === use_property.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useState } from "react";

import type { ObjectHandle, Var } from "../index.js";

/**
 * Subscribe to one property on an object. Returns `undefined` until the first server push,
 * then the latest value. Unsubscribes on unmount or when `obj` / `name` change.
 */
export function useProperty(obj: ObjectHandle | null, name: string): Var | undefined {
  const [value, setValue] = useState<Var | undefined>(undefined);

  useEffect(() => {
    if (!obj) return;
    setValue(undefined);
    return obj.onPropertyChanged(name, setValue);
  }, [obj, name]);

  return value;
}
