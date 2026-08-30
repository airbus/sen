// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// included record fom
#include "fom/module-a.xml.h"

int main()
{
  constexpr fom::DerivedRecord record {{45}, 67};

  static_assert(record.inherited == 45);
  static_assert(record.own == 67);

  return 0;
}
