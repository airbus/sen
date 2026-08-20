// === variant_conversion.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "variant_conversion.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/var.h"

// pybind
#include <pybind11/cast.h>
// NOLINTNEXTLINE(misc-include-cleaner): registers std::chrono <-> datetime.timedelta casters by inclusion
#include <pybind11/chrono.h>
#include <pybind11/pytypes.h>

// std
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>

namespace
{

[[nodiscard]] bool typeNeedsAdaptation(const sen::Type& type);

[[nodiscard]] bool computeNeedsAdaptation(const sen::Type& type)
{
  if (type.isVariantType())
  {
    return true;
  }
  if (const auto* s = type.asStructType())
  {
    for (const auto& field: s->getFields())
    {
      if (typeNeedsAdaptation(*field.type))
      {
        return true;
      }
    }
    return false;
  }
  if (const auto* s = type.asSequenceType())
  {
    return typeNeedsAdaptation(*s->getElementType());
  }
  if (const auto* a = type.asAliasType())
  {
    return typeNeedsAdaptation(*a->getAliasedType());
  }
  if (const auto* o = type.asOptionalType())
  {
    return typeNeedsAdaptation(*o->getType());
  }
  return false;
}

[[nodiscard]] bool typeNeedsAdaptation(const sen::Type& type)
{
  // Key by Type*, not by MemberHash: MemberHash is 32-bit and two distinct types could
  // alias; a false negative here would silently corrupt variant tags. Type* is stable for
  // the registry's lifetime (same rationale as lookupProperty's ClassType* cache in
  // module.cpp). Stale entries after a registry closes are harmless -- they just never get
  // hit again.
  static std::unordered_map<const sen::Type*, bool> cache;
  if (auto it = cache.find(&type); it != cache.end())
  {
    return it->second;
  }
  // Provisional true on cycles is conservative: if a recursion lands back on itself we may
  // adapt unnecessarily, but never skip when we should adapt (which would silently corrupt
  // variant tags). Optimistic false here would propagate wrong answers to siblings via the
  // cache.
  cache.emplace(&type, true);
  const auto result = computeNeedsAdaptation(type);
  cache[&type] = result;
  return result;
}

[[nodiscard]] sen::Var deepClone(const sen::Var& var)
{
  return std::visit(
    sen::Overloaded {
      [](const std::monostate& /*val*/) -> sen::Var { return {}; },
      // Catch-all by value covers the trivially-copyable scalar arms (bool, ints, floats).
      [](auto val) -> sen::Var { return val; },
      // std::string explicitly by const-ref so the catch-all doesn't copy the buffer twice.
      [](const std::string& val) -> sen::Var { return val; },
      [](const sen::Duration& val) -> sen::Var { return val; },
      [](const sen::TimeStamp& val) -> sen::Var { return val; },
      [](const sen::VarList& val) -> sen::Var
      {
        sen::VarList copy;
        copy.reserve(val.size());
        for (const auto& elem: val)
        {
          copy.push_back(deepClone(elem));
        }
        return copy;
      },
      [](const sen::VarMap& val) -> sen::Var
      {
        sen::VarMap copy;
        for (const auto& [k, v]: val)
        {
          copy.try_emplace(k, deepClone(v));
        }
        return copy;
      },
      [](const sen::KeyedVar& val) -> sen::Var
      { return sen::KeyedVar {std::get<0>(val), std::make_shared<sen::Var>(deepClone(*std::get<1>(val)))}; }},
    static_cast<const ::sen::Var::ValueType&>(var));
}

}  // namespace

pybind11::object visitVar(const sen::Var& var)
{
  return std::visit(
    sen::Overloaded {[](const std::monostate& /*val*/) -> pybind11::object { return pybind11::none(); },
                     // Catch-all by value covers the trivially-copyable scalar arms.
                     [](auto val) -> pybind11::object { return pybind11::cast(val); },
                     // std::string explicitly by const-ref so the catch-all doesn't copy.
                     [](const std::string& val) -> pybind11::object { return pybind11::cast(val); },
                     [](const sen::Duration& val) -> pybind11::object { return pybind11::cast(val.toChrono()); },
                     [](const sen::TimeStamp& val) -> pybind11::object
                     { return pybind11::cast(val.sinceEpoch().toChrono()); },
                     [](const sen::VarList& val) -> pybind11::object
                     {
                       pybind11::list result;
                       for (const auto& elem: val)
                       {
                         result.append(visitVar(elem));
                       }
                       return result;
                     },
                     [](const sen::VarMap& val) -> pybind11::object
                     {
                       pybind11::dict result;
                       for (const auto& [first, second]: val)
                       {
                         result[first.c_str()] = visitVar(second);
                       }
                       return result;
                     },
                     [](const sen::KeyedVar& val) -> pybind11::object
                     {
                       // Reached only when caller did not supply a Type; tag stays as the numeric index.
                       pybind11::dict result;
                       result["type"] = std::to_string(std::get<0>(val));
                       result["value"] = visitVar(*std::get<1>(val));
                       return result;
                     }},
    static_cast<const ::sen::Var::ValueType&>(var));
}

pybind11::object toPython(const sen::Var& var, const sen::Type* type)
{
  if (type != nullptr && typeNeedsAdaptation(*type))
  {
    // KeyedVar inners are shared_ptr-aliased with the recording's storage; adapt a clone.
    auto clone = deepClone(var);
    std::ignore = sen::impl::adaptVariant(*type, clone, std::nullopt, /*useStrings=*/true);
    return visitVar(clone);
  }
  return visitVar(var);
}
