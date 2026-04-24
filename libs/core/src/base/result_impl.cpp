// === result_impl.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"

// std
#include <cstdio>
#include <string_view>

namespace sen::impl
{

void resultExpect(const bool value, const std::string_view errorMsg) noexcept
{
  if (!value)
  {
    fputs("Result error: ", stderr);
    if (!errorMsg.empty() && errorMsg.data() != nullptr)
    {
      fwrite(errorMsg.data(), sizeof(char), errorMsg.size(), stderr);
    }
    fflush(stderr);
  }

  SEN_EXPECT(value);
}

}  // namespace sen::impl
