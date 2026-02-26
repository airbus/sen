// === object_source_wrapper.h =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_PY_SRC_OBJECT_SOURCE_WRAPPER_H
#define SEN_COMPONENTS_PY_SRC_OBJECT_SOURCE_WRAPPER_H

// component
#include "object_wrapper.h"

// pybind
#include <pybind11/pybind11.h>

// sen
#include "sen/core/obj/object_source.h"

namespace sen::components::py
{

class RunApi;

/// Wraps an object source to offer it to the Python world
class ObjectSourceWrapper
{
  SEN_MOVE_ONLY(ObjectSourceWrapper)

public:
  ObjectSourceWrapper(std::shared_ptr<ObjectSource> source, RunApi* api);
  ~ObjectSourceWrapper() = default;

private:
  void add(const ObjectWrapper& object);
  void remove(const ObjectWrapper& object);

public:
  static void definePythonApi(pybind11::module_& pythonModule);

private:
  std::shared_ptr<ObjectSource> source_;
  RunApi* api_;
};

}  // namespace sen::components::py

#endif  // SEN_COMPONENTS_PY_SRC_OBJECT_SOURCE_WRAPPER_H
