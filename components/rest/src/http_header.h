// === http_header.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_HTTP_HEADER_H
#define SEN_COMPONENTS_REST_SRC_HTTP_HEADER_H

// std
#include <array>
#include <cstddef>
#include <string>

namespace sen::components::rest
{

/// HttpHeader class holding header field name and its value.
class HttpHeader
{
public:
  static constexpr size_t maxHeaderFieldLen = 64;
  static constexpr size_t maxHeaderValueLen = 256;

public:
  HttpHeader() = default;
  HttpHeader(const std::string& field, const std::string& value);

public:
  /// Returns HTTP header field name
  [[nodiscard]] const std::array<char, maxHeaderFieldLen>& field() const;

  /// Returns HTTP header field value
  [[nodiscard]] const std::array<char, maxHeaderValueLen>& value() const;

private:
  std::array<char, maxHeaderFieldLen> field_;
  std::array<char, maxHeaderValueLen> value_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_HTTP_HEADER_H
