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
#include "sen/core/meta/var.h"

// pybind
#include <object.h>
#include <pybind11/cast.h>
#include <pybind11/pytypes.h>

// std
#include <string>
#include <variant>

pybind11::object toPython(const sen::Var& var)
{
  return std::visit(
    sen::Overloaded {
      [](const std::monostate& /*val*/) -> pybind11::object { return pybind11::cast<pybind11::none>(Py_None); },
      [](auto val) -> pybind11::object { return pybind11::cast(val); },
      [](const sen::Duration& val) -> pybind11::object { return pybind11::cast(val.toChrono()); },
      [](const sen::TimeStamp& val) -> pybind11::object { return pybind11::cast(val.sinceEpoch().toChrono()); },
      [](const sen::VarList& val) -> pybind11::object
      {
        pybind11::list result;
        for (const auto& elem: val)
        {
          result.append(toPython(elem));
        }
        return result;
      },
      [](const sen::VarMap& val) -> pybind11::object
      {
        pybind11::dict result;
        for (const auto& [first, second]: val)
        {
          result[first.c_str()] = toPython(second);
        }
        return result;
      },
      [](const sen::KeyedVar& val) -> pybind11::object
      {
        pybind11::dict result;
        result["type"] = std::to_string(std::get<0>(val));
        result["value"] = toPython(*std::get<1>(val));
        return result;
      }},
    static_cast<const ::sen::Var::ValueType&>(var));
}
