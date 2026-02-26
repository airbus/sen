// === type_specs_utils.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_TYPE_SPECS_UTILS_H
#define SEN_KERNEL_TYPE_SPECS_UTILS_H

// sen
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"

// stl
#include "stl/sen/kernel/type_specs.stl.h"

namespace sen::kernel
{

/// \addtogroup kernel
/// @{

/// Implementation detail for the kernel.
[[nodiscard]] CustomTypeSpec makeCustomTypeSpec(const CustomType* type);

/// Checks if two meta types are equivalent (same hash)
[[nodiscard]] bool equivalent(const Type* localType, const Type* remoteType);

/// Implementation detail for the kernel.
[[nodiscard]] ConstTypeHandle<CustomType> buildNonNativeType(const CustomTypeSpec& remoteType,
                                                             const CustomTypeRegistry& nativeTypes,
                                                             const CustomTypeRegistry& nonNativeTypes);

/// Detects which differences cause
[[nodiscard]] std::vector<std::string> getRuntimeDifferences(const Type* localType, const Type* remoteType);

/// Checks if two types are runtime-compatible and returns the error string in case they are not
[[nodiscard]] std::vector<std::string> runtimeCompatible(const Type* localType, const Type* remoteType);

/// Translates a CustomTypeSpec from V4 to the current version (V6) of the kernel protocol
[[nodiscard]] CustomTypeSpec toCurrentVersion(const CustomTypeSpecV4& v4);

/// Translates a CustomTypeSpec from V5 to the current version (V6) of the kernel protocol
[[nodiscard]] CustomTypeSpec toCurrentVersion(const CustomTypeSpecV5& v5);

/// @}

}  // namespace sen::kernel

#endif  // SEN_KERNEL_TYPE_SPECS_UTILS_H
