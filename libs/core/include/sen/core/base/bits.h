// === bits.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_BITS_H
#define SEN_CORE_BASE_BITS_H

#if defined(__cpp_lib_bit_cast)
#  include <bit>
#endif

// std
#include <cstring>
#include <type_traits>

#if defined(__cpp_lib_bit_cast)
namespace sen::std_util
{
using std::bit_cast;
}  // namespace sen::std_util
#else
namespace sen::std_util
{

// Forward implementation of std::bit_cast from C++20.
// Impl provided by: https://en.cppreference.com/w/cpp/numeric/bit_cast.html
template <class To, class From>
std::enable_if_t<sizeof(To) == sizeof(From) && std::is_trivially_copyable_v<From> && std::is_trivially_copyable_v<To>,
                 To>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
bit_cast(const From& src) noexcept
{
  static_assert(std::is_trivially_constructible_v<To>,
                "This implementation additionally requires "
                "destination type to be trivially constructible");

  To dst;
  std::memcpy(&dst, &src, sizeof(To));
  return dst;
}

}  // namespace sen::std_util
#endif

#endif  // SEN_CORE_BASE_BITS_H
