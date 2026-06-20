// === type_casters.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_DB_BINDINGS_PYTHON_TYPE_CASTERS_H
#define SEN_LIBS_DB_BINDINGS_PYTHON_TYPE_CASTERS_H

// sen
#include "sen/core/obj/object.h"

// pybind
#include <pybind11/cast.h>  // PYBIND11_TYPE_CASTER, const_name

// std
#include <cstdint>
#include <limits>

namespace pybind11::detail
{

template <>
struct type_caster<sen::ObjectId>
{
public:
  PYBIND11_TYPE_CASTER(sen::ObjectId, const_name("ObjectId"));

  // Accept only Python ints. PyNumber_Long would silently coerce bools/strings/bytes
  // (True -> ObjectId(1), "5" -> ObjectId(5)), which is the wrong policy for an id-shaped
  // strong type.
  bool load(handle src, bool /*convert*/)
  {
    if (PyLong_Check(src.ptr()) == 0)
    {
      return false;
    }
    const unsigned long raw = PyLong_AsUnsignedLong(src.ptr());
    if (raw == static_cast<unsigned long>(-1) && (PyErr_Occurred() != nullptr))
    {
      PyErr_Clear();
      return false;
    }
    if (raw > std::numeric_limits<std::uint32_t>::max())
    {
      return false;
    }
    value = sen::ObjectId(static_cast<std::uint32_t>(raw));
    return true;
  }

  static handle cast(sen::ObjectId src, return_value_policy /* policy */, handle /* parent */)
  {
    return PyLong_FromUnsignedLong(src.get());
  }
};

}  // namespace pybind11::detail

#endif  // SEN_LIBS_DB_BINDINGS_PYTHON_TYPE_CASTERS_H
