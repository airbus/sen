// === type_utils.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_TYPE_UTILS_H
#define SEN_CORE_META_TYPE_UTILS_H

#include <type_traits>

namespace sen
{
#ifdef __cpp_lib_remove_cvref
using std::remove_cvref;
using std::remove_cvref_t;
#else
template <typename T>
struct remove_cvref  // NOLINT(readability-identifier-naming)
{
  using type = std::remove_cv_t<std::remove_reference_t<T>>;  // NOLINT(readability-identifier-naming)
};
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;  // NOLINT(readability-identifier-naming)
#endif
}  // namespace sen

#endif  // SEN_CORE_META_TYPE_UTILS_H
