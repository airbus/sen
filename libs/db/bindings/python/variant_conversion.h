// === variant_conversion.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_DB_BINDINGS_PYTHON_VARIANT_CONVERSION_H
#define SEN_LIBS_DB_BINDINGS_PYTHON_VARIANT_CONVERSION_H

// sen
#include "sen/core/meta/var.h"

// pybind
#include <pybind11/pybind11.h>  // NOLINT(misc-include-cleaner)
#include <pybind11/pytypes.h>

[[nodiscard]] pybind11::object toPython(const sen::Var& var);

#endif  // SEN_LIBS_DB_BINDINGS_PYTHON_VARIANT_CONVERSION_H
