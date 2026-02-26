// === basic_traits.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_BASIC_TRAITS_H
#define SEN_CORE_META_BASIC_TRAITS_H

#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/type.h"

namespace sen
{

struct Var;

/// \addtogroup traits
/// @{

/// Helper class that traits classes can inherit from when the
/// type at hand is basic (native or trivial).
///
/// The idea is to use it as follows:
/// \code{.cpp}
/// template <>
/// struct *Traits<MyType>: public BasicTraits<MyType>
/// \endcode
template <typename T>
struct BasicTraits
{
  static constexpr bool available = true;

  static void variantToValue(const Var& var, T& val) { val = getCopyAs<T>(var); }
  static void valueToVariant(T val, Var& var) { var = val; }
  static constexpr uint32_t serializedSize(T val) noexcept { return impl::getSerializedSize(val); }
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_BASIC_TRAITS_H
