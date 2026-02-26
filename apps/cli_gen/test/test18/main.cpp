// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// repeated fom
#include "fom/module-a.xml.h"
#include "fom/module-b.xml.h"

int main()
{
  try
  {
    constexpr fom::RepeatedInteractionA type1 {45};
    constexpr fom::RepeatedInteractionB type2 {67};

    static_assert(type1.testParameter == 45);
    static_assert(type2.testParameter == 67);

    return 0;
  }
  catch (...)
  {
    return 1;
  }
}
