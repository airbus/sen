// === type_impl.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_DETAIL_TYPE_IMPL_H
#define SEN_CORE_META_DETAIL_TYPE_IMPL_H

// NOLINTNEXTLINE
#define DECL_IS_TYPE_FUNC(type_name)                                                                                   \
  [[nodiscard]] inline bool is##type_name() const noexcept { return as##type_name() != nullptr; }                      \
  [[nodiscard]] virtual const type_name* as##type_name() const noexcept { return nullptr; }

#endif  // SEN_CORE_META_DETAIL_TYPE_IMPL_H
