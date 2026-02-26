// === http_header.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "http_header.h"

// std
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace sen::components::rest
{

HttpHeader::HttpHeader(const std::string& field, const std::string& value)
{
  field_.fill('\0');
  std::size_t nameSize = std::min(field.size(), HttpHeader::maxHeaderFieldLen);
  std::copy_n(field.begin(), nameSize, field_.begin());

  value_.fill('\0');
  std::size_t valueSize = std::min(value.size(), HttpHeader::maxHeaderValueLen);
  std::copy_n(value.begin(), valueSize, value_.begin());
}

[[nodiscard]] const std::array<char, HttpHeader::maxHeaderFieldLen>& HttpHeader::field() const { return field_; }

[[nodiscard]] const std::array<char, HttpHeader::maxHeaderValueLen>& HttpHeader::value() const { return value_; }

}  // namespace sen::components::rest
