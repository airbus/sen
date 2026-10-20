// === base64.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/base64.h"

// std
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sen::std_util
{

std::string base64Encode(std::string_view input)
{
  constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((input.size() + 2U) / 3U) * 4U);

  auto byteAt = [&](std::size_t k)
  { return k < input.size() ? static_cast<std::uint32_t>(static_cast<unsigned char>(input[k])) : 0U; };

  for (std::size_t i = 0U; i < input.size(); i += 3U)
  {
    const std::uint32_t triple = (byteAt(i) << 16U) | (byteAt(i + 1U) << 8U) | byteAt(i + 2U);
    out.push_back(alphabet[(triple >> 18U) & 0x3FU]);
    out.push_back(alphabet[(triple >> 12U) & 0x3FU]);
    out.push_back(i + 1U < input.size() ? alphabet[(triple >> 6U) & 0x3FU] : '=');
    out.push_back(i + 2U < input.size() ? alphabet[triple & 0x3FU] : '=');
  }

  return out;
}

}  // namespace sen::std_util
