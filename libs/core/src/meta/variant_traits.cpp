// === variant_traits.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/variant_traits.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// std
#include <cstdint>
#include <string>
#include <tuple>

namespace sen
{

void VariantTraitsBaseBase::expectAtLeastOneField(const char* name, const Span<uint16_t>& fields)
{
  if (fields.empty())
  {
    std::string err;
    err.append("no field index for variant '");
    err.append(name);
    err.append("'");
    throwRuntimeError(err);
  }
}

[[noreturn]] void VariantTraitsBaseBase::throwNonNativeField(const char* variantName, const char* fieldName)
{
  std::string err;
  err.append("field '");
  err.append(fieldName);
  err.append("' of variant '");
  err.append(variantName);
  err.append("' has not a native type");
  throwRuntimeError(err);
}

[[noreturn]] void VariantTraitsBaseBase::throwInvalidFieldIndex(const char* variantName, uint16_t index)
{
  std::string err;
  err.append("variant '");
  err.append(variantName);
  err.append("' has no field of index ");
  err.append(std::to_string(index));
  throwRuntimeError(err);
}

std::tuple<const char*, const Var*> VariantTraitsBaseBase::getTypeAndValue(const Var& var,
                                                                           ConstTypeHandle<VariantType> meta)
{
  const auto* map = var.getIf<VarMap>();
  if (map == nullptr)
  {
    return {nullptr, nullptr};
  }

  auto typeItr = map->find("type");
  if (typeItr == map->end())
  {
    return {nullptr, nullptr};
  }

  const Var* valuePtr = nullptr;
  auto valueItr = map->find("value");
  if (valueItr != map->end())
  {
    valuePtr = &valueItr->second;
  }

  if (typeItr->second.holds<std::string>())
  {
    return {typeItr->second.get<std::string>().c_str(), valuePtr};
  }
  return {meta->getFieldFromKey(typeItr->second.getCopyAs<uint32_t>())->type->getName().data(), valuePtr};
}

}  // namespace sen
