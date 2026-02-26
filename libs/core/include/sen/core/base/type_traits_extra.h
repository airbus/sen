// === type_traits_extra.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_TYPE_TRAITS_EXTRA_H
#define SEN_CORE_BASE_TYPE_TRAITS_EXTRA_H

// std
#include <type_traits>

namespace sen::std_util
{

template <template <typename...> class Template, typename T>
struct IsInstantiationOf: std::false_type
{
};

template <template <typename...> class Template, typename... Args>
struct IsInstantiationOf<Template, Template<Args...>>: std::true_type
{
};

template <template <typename...> class Template, typename T>
constexpr bool isInstantiationOfV = IsInstantiationOf<Template, T>::value;

}  // namespace sen::std_util

#endif  // SEN_CORE_BASE_TYPE_TRAITS_EXTRA_H
