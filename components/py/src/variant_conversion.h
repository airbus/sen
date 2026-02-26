// === variant_conversion.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_PY_SRC_VARIANT_CONVERSION_H
#define SEN_COMPONENTS_PY_SRC_VARIANT_CONVERSION_H

// sen
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// pybind
#include <pybind11/chrono.h>    // NOLINT(misc-include-cleaner)
#include <pybind11/embed.h>     // NOLINT(misc-include-cleaner)
#include <pybind11/pybind11.h>  // NOLINT(misc-include-cleaner)
#include <pybind11/pytypes.h>

namespace sen::components::py
{

/// How to represent durations in the Python world.
using PythonDuration = std::chrono::duration<int64_t, std::nano>;

/// Transforms a Var into a Python object.
pybind11::object toPython(const Var& var);

/// Transforms a Python object into a var, given an expected type.
Var fromPython(const pybind11::object& obj, const Type& type);

}  // namespace sen::components::py

#endif  // SEN_COMPONENTS_PY_SRC_VARIANT_CONVERSION_H
