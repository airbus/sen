// === object_list_wrapper.h ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_PY_SRC_OBJECT_LIST_WRAPPER_H
#define SEN_COMPONENTS_PY_SRC_OBJECT_LIST_WRAPPER_H

// component
#include "object_wrapper.h"

// pybind
#include <pybind11/pytypes.h>

// sen
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"

namespace sen::components::py
{

class RunApi;

/// Wraps an object list to offer it to the Python world
class ObjectListWrapper
{
  SEN_NOCOPY_NOMOVE(ObjectListWrapper)

public:
  ObjectListWrapper(std::shared_ptr<ObjectSource> source,
                    std::shared_ptr<ObjectProvider> provider,
                    std::string providerName,
                    RunApi* api);
  ~ObjectListWrapper();

public:
  static void definePythonApi(pybind11::module_& self);

private:
  inline void callBack(const ObjectWrapper& wrapper, pybind11::object& func) const;

private:
  using WrapperList = std::vector<ObjectWrapper>;

private:
  ObjectList<Object> list_;
  std::shared_ptr<ObjectSource> source_;
  std::shared_ptr<ObjectProvider> provider_;
  std::string providerName_;
  RunApi* api_;
  WrapperList wrappers_;
  pybind11::object onAddedCallback_;
  pybind11::object onRemovedCallback_;
};

}  // namespace sen::components::py

#endif  // SEN_COMPONENTS_PY_SRC_OBJECT_LIST_WRAPPER_H
