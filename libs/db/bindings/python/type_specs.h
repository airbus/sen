// === type_specs.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_PYTHON_TYPE_SPECS_H
#define SEN_DB_PYTHON_TYPE_SPECS_H

#include <pybind11/pytypes.h>

namespace sen
{
class CustomType;
}

namespace sen::db::python
{

// Python dict describing a CustomType using the kernel external encoding (string-tagged
// variants). Matches the JSON-RPC `getType` shape.
[[nodiscard]] pybind11::object customTypeToPython(const sen::CustomType& type);

}  // namespace sen::db::python

#endif  // SEN_DB_PYTHON_TYPE_SPECS_H
