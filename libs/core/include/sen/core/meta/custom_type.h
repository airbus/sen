// === custom_type.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_CUSTOM_TYPE_H
#define SEN_CORE_META_CUSTOM_TYPE_H

// sen
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"

namespace sen
{

/// \addtogroup types
/// @{

/// Abstract base class for all user-defined types (`ClassType`, `EnumType`, `StructType`, `VariantType`).
///
/// A `CustomType` extends `Type` with a mandatory fully-qualified name that uniquely identifies
/// it within a type registry. Concrete subclasses are constructed through their respective
/// `make()` factory functions.
class CustomType: public Type
{
  SEN_META_TYPE(CustomType)

public:
  /// @return Fully-qualified name of this type, including namespace (e.g. `"ns.MyClass"`).
  [[nodiscard]] virtual std::string_view getQualifiedName() const noexcept = 0;

protected:
  /// @param hash Pre-computed 32-bit hash that identifies this type instance.
  explicit CustomType(MemberHash hash) noexcept;
  ~CustomType() noexcept override = default;
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_CUSTOM_TYPE_H
