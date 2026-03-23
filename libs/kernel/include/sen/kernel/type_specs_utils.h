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

/// Serialises a `CustomType` into a `CustomTypeSpec` wire message for protocol exchange.
/// @param type  The resolved custom type to serialise; must not be null.
/// @return A `CustomTypeSpec` describing the type's structure in the kernel wire format.
[[nodiscard]] CustomTypeSpec makeCustomTypeSpec(const CustomType* type);

/// Returns `true` if @p localType and @p remoteType have the same content hash.
/// A hash match means the two type descriptors are structurally identical.
/// @param localType   Type descriptor from the local process.
/// @param remoteType  Type descriptor received from a remote process.
/// @return `true` if both types share the same hash; `false` otherwise.
[[nodiscard]] bool equivalent(const Type* localType, const Type* remoteType);

/// Reconstructs a `CustomType` from a remote `CustomTypeSpec`, resolving references
/// against the provided native and non-native type registries.
/// @param remoteType     Wire-format type specification received from a remote process.
/// @param nativeTypes    Registry of locally-known types used for reference resolution.
/// @param nonNativeTypes Registry of previously-built non-native types for cycle resolution.
/// @return A `ConstTypeHandle<CustomType>` for the reconstructed type.
[[nodiscard]] ConstTypeHandle<CustomType> buildNonNativeType(const CustomTypeSpec& remoteType,
                                                             const CustomTypeRegistry& nativeTypes,
                                                             const CustomTypeRegistry& nonNativeTypes);

/// Returns the list of semantic differences between @p localType and @p remoteType that
/// would cause runtime incompatibilities (e.g. field reordering, type changes).
/// @param localType   Type descriptor from the local process.
/// @param remoteType  Type descriptor received from a remote process.
/// @return Vector of human-readable difference descriptions; empty if types are compatible.
[[nodiscard]] std::vector<std::string> getRuntimeDifferences(const Type* localType, const Type* remoteType);

/// Checks whether @p localType and @p remoteType are runtime-compatible.
/// @param localType   Type descriptor from the local process.
/// @param remoteType  Type descriptor received from a remote process.
/// @return Empty vector if compatible; otherwise a list of incompatibility descriptions.
[[nodiscard]] std::vector<std::string> runtimeCompatible(const Type* localType, const Type* remoteType);

/// Upgrades a V4 `CustomTypeSpec` to the current (V6) kernel protocol format.
/// @param v4  The legacy V4 type specification to convert.
/// @return An equivalent `CustomTypeSpec` in the current protocol version.
[[nodiscard]] CustomTypeSpec toCurrentVersion(const CustomTypeSpecV4& v4);

/// Upgrades a V5 `CustomTypeSpec` to the current (V6) kernel protocol format.
/// @param v5  The legacy V5 type specification to convert.
/// @return An equivalent `CustomTypeSpec` in the current protocol version.
[[nodiscard]] CustomTypeSpec toCurrentVersion(const CustomTypeSpecV5& v5);

/// @}

}  // namespace sen::kernel

#endif  // SEN_KERNEL_TYPE_SPECS_UTILS_H
