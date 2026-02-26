// === struct_traits.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_STRUCT_TRAITS_H
#define SEN_CORE_META_STRUCT_TRAITS_H

#include "sen/core/meta/struct_type.h"

namespace sen
{

/// \addtogroup traits
/// @{

/// Base class for struct traits.
struct StructTraitsBase
{
  static constexpr bool available = true;

protected:
  static void expectAtLeastOneField(const char* name, const Span<uint16_t>& fields);
  [[noreturn]] static void throwEmptyStructError(const char* name);
  [[noreturn]] static void throwNonNativeField(const char* structName, const char* fieldName);
  [[noreturn]] static void throwInvalidFieldIndex(const char* structName, uint16_t index);
  [[nodiscard]] static const VarMap& getMap(const Var& var, std::string_view structType);
  template <typename T>
  static void extractField(const VarMap& map,
                           T& val,
                           std::string_view fieldName,
                           std::string_view fieldType,
                           std::string_view structType);
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline void StructTraitsBase::throwEmptyStructError(const char* name)
{
  std::string err;
  err.append("struct '");
  err.append(name);
  err.append("' has no fields");
  throwRuntimeError(err);
}

inline void StructTraitsBase::expectAtLeastOneField(const char* name, const Span<uint16_t>& fields)
{
  if (fields.empty())
  {
    std::string err;
    err.append("no field index for struct '");
    err.append(name);
    err.append("'");
    throwRuntimeError(err);
  }
}

inline void StructTraitsBase::throwNonNativeField(const char* structName, const char* fieldName)
{
  std::string err;
  err.append("field '");
  err.append(fieldName);
  err.append("' of struct '");
  err.append(structName);
  err.append("' has not a native type");
  throwRuntimeError(err);
}

inline void StructTraitsBase::throwInvalidFieldIndex(const char* structName, uint16_t index)
{
  std::string err;
  err.append("struct '");
  err.append(structName);
  err.append("' has no field of index ");
  err.append(std::to_string(index));
  throwRuntimeError(err);
}

inline const VarMap& StructTraitsBase::getMap(const Var& var, std::string_view structType)
{
  static VarMap emptyMap = {};

  if (var.isEmpty())
  {
    return emptyMap;
  }

  if (var.holds<VarMap>())
  {
    return var.get<VarMap>();
  }

  std::string err;
  err.append("struct ");
  err.append(structType);
  err.append(": variant does not hold a variant map (");
  err.append(var.getCopyAs<std::string>());
  err.append(")");
  throw std::runtime_error(err);
}

template <typename T>
inline void StructTraitsBase::extractField(const VarMap& map,
                                           T& val,
                                           std::string_view fieldName,
                                           std::string_view fieldType,
                                           std::string_view structType)
{
  std::ignore = fieldType;
  std::ignore = structType;

  if (const auto itr = map.find(fieldName); itr != map.end())
  {
    ::sen::VariantTraits<T>::variantToValue(itr->second, val);
  }
}

}  // namespace sen

#endif  // SEN_CORE_META_STRUCT_TRAITS_H
