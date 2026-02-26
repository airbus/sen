// === variant_conversion.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "variant_conversion.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/detail/types_fwd.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// pybind11
#include <object.h>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pytypes.h>

// std
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace sen::components::py
{

namespace
{

// -------------------------------------------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------------------------------------------

class FromPythonConverter: public TypeVisitor
{
  SEN_NOCOPY_NOMOVE(FromPythonConverter)

public:
  explicit FromPythonConverter(const pybind11::object& obj): pyObj_(obj) {}

  ~FromPythonConverter() override = default;

public:
  [[nodiscard]] Var&& extractResult() noexcept { return std::move(result_); }

public:
  void apply(const Type& type) override
  {
    std::string err;
    err.append("expecting type ");
    err.append(type.getName());
    throw pybind11::cast_error(err);
  }

  void apply(const EnumType& type) override
  {
    auto str = pyObj_.cast<std::string>();

    const auto* elem = type.getEnumFromName(str);
    if (elem == nullptr)
    {
      std::string err;
      err.append("invalid enumeration ");
      err.append(str);
      err.append("for enum type ");
      err.append(type.getQualifiedName());
      throw pybind11::cast_error(err);
    }

    result_ = elem->key;
  }

  void apply(const BoolType& /*type*/) override { result_ = pyObj_.cast<bool>(); }

  void apply(const UInt8Type& /*type*/) override { result_ = pyObj_.cast<uint8_t>(); }

  void apply(const Int16Type& /*type*/) override { result_ = pyObj_.cast<int16_t>(); }

  void apply(const UInt16Type& /*type*/) override { result_ = pyObj_.cast<uint16_t>(); }

  void apply(const Int32Type& /*type*/) override { result_ = pyObj_.cast<int32_t>(); }

  void apply(const UInt32Type& /*type*/) override { result_ = pyObj_.cast<uint32_t>(); }

  void apply(const Int64Type& /*type*/) override { result_ = pyObj_.cast<int64_t>(); }

  void apply(const UInt64Type& /*type*/) override { result_ = pyObj_.cast<uint64_t>(); }

  void apply(const Float32Type& /*type*/) override { result_ = pyObj_.cast<float32_t>(); }

  void apply(const Float64Type& /*type*/) override { result_ = pyObj_.cast<float64_t>(); }

  void apply(const DurationType& /*type*/) override { result_ = pyObj_.cast<PythonDuration>(); }

  void apply(const TimestampType& /*type*/) override { result_ = pyObj_.cast<PythonDuration>(); }

  void apply(const StringType& /*type*/) override { result_ = pyObj_.cast<std::string>(); }

  void apply(const StructType& type) override
  {
    VarMap map;
    const auto dict = pyObj_.cast<pybind11::dict>();

    if (dict.size() != type.getFields().size())
    {
      std::string err;
      err.append("python dictionary size differs from field count in ");
      err.append(type.getQualifiedName());
      throw pybind11::cast_error(err);
    }

    for (const auto& field: type.getFields())
    {
      map.try_emplace(field.name, fromPython(dict[field.name.c_str()], *field.type));
    }

    result_ = std::move(map);
  }

  void apply(const VariantType& type) override
  {
    const auto dict = pyObj_.cast<pybind11::dict>();

    // get the field corresponding with the type
    auto typeName = dict["type"].cast<std::string>();
    auto itr = std::find_if(type.getFields().begin(),                                             // NOSONAR
                            type.getFields().end(),                                               // NOSONAR
                            [&](const auto& elem) { return elem.type->getName() == typeName; });  // NOSONAR

    if (itr == type.getFields().end())
    {
      std::string err;
      err.append("type ");
      err.append(typeName);
      err.append(" is not supported by variant ");
      err.append(type.getQualifiedName());

      throw pybind11::cast_error(err);
    }

    result_ = KeyedVar(itr->key, std::make_shared<Var>(fromPython(dict["value"], *itr->type)));
  }

  void apply(const SequenceType& type) override
  {
    const auto seq = pyObj_.cast<pybind11::sequence>();

    VarList list;
    list.reserve(seq.size());

    for (const auto& elem: seq)
    {
      list.push_back(fromPython(elem, *type.getElementType()));
    }

    result_ = std::move(list);
  }

  void apply(const QuantityType& type) override { result_ = fromPython(pyObj_, *type.getElementType()); }

  void apply(const AliasType& type) override { result_ = fromPython(pyObj_, *type.getAliasedType()); }

  void apply(const OptionalType& type) override
  {
    if (pyObj_.is_none())
    {
      result_ = {};
    }
    else
    {
      result_ = fromPython(pyObj_, *type.getType());
    }
  }

private:
  const pybind11::object& pyObj_;
  Var result_;
};

}  // namespace

// -------------------------------------------------------------------------------------------------------------
// Main function
// -------------------------------------------------------------------------------------------------------------

pybind11::object toPython(const Var& var)
{
  return std::visit(
    Overloaded {[](const std::monostate& /*val*/) -> pybind11::object
                { return pybind11::cast<pybind11::none>(Py_None); },
                [](auto val) -> pybind11::object { return pybind11::cast(val); },
                [](const Duration& val) -> pybind11::object { return pybind11::cast(val.toChrono()); },
                [](const TimeStamp& val) -> pybind11::object { return pybind11::cast(val.sinceEpoch().toChrono()); },
                [](const VarList& val) -> pybind11::object
                {
                  pybind11::list result;
                  for (const auto& elem: val)
                  {
                    result.append(toPython(elem));
                  }
                  return result;
                },
                [](const VarMap& map) -> pybind11::object
                {
                  pybind11::dict result;
                  for (const auto& [key, value]: map)
                  {
                    result[key.c_str()] = toPython(value);
                  }
                  return result;
                },
                [](const KeyedVar& val) -> pybind11::object
                {
                  pybind11::dict result;
                  result["type"] = std::to_string(std::get<0>(val));
                  result["value"] = toPython(*std::get<1>(val));
                  return result;
                }},
    static_cast<const ::sen::Var::ValueType&>(var));
}

Var fromPython(const pybind11::object& obj, const Type& type)
{
  FromPythonConverter converter(obj);
  type.accept(converter);
  return converter.extractResult();
}

}  // namespace sen::components::py
