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
#include <pybind11/stl.h>  // NOLINT(misc-include-cleaner)

namespace pybind11::detail
{

template <>
struct type_caster<sen::ObjectId>
{
public:
  PYBIND11_TYPE_CASTER(sen::ObjectId, const_name("ObjectId"));

  bool load(handle src, bool ignore)
  {
    std::ignore = ignore;
    PyObject* source = src.ptr();
    PyObject* tmp = PyNumber_Long(source);
    if (!tmp)
    {
      return false;
    }

    value = sen::ObjectId(PyLong_AsLong(tmp));
    Py_DECREF(tmp);
    return !PyErr_Occurred();  // NOLINT
  }

  static handle cast(sen::ObjectId src, return_value_policy /* policy */, handle /* parent */)
  {
    return PyLong_FromLong(src.get());
  }
};

}  // namespace pybind11::detail

#endif  // SEN_LIBS_DB_BINDINGS_PYTHON_TYPE_CASTERS_H
