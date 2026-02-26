// === object_source_wrapper.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_source_wrapper.h"

// component
#include "component_api.h"
#include "object_wrapper.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/obj/object_source.h"

// pybind11
#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>

// std
#include <memory>
#include <string>
#include <utility>

namespace sen::components::py
{

ObjectSourceWrapper::ObjectSourceWrapper(std::shared_ptr<ObjectSource> source, RunApi* api)
  : source_(std::move(source)), api_(api)
{
  SEN_ASSERT(source_.get() != nullptr);
  SEN_ASSERT(api_ != nullptr);
}

void ObjectSourceWrapper::add(const ObjectWrapper& object)
{
  if (!object.getNative())
  {
    std::string err;
    err.append("object ");
    err.append(object.get()->getName());
    err.append(" is not owned by this component");
    throw pybind11::attribute_error(err);
  }

  source_->add(object.getNative());

  if (api_->getSyncCalls())
  {
    api_->commit();
  }
}

void ObjectSourceWrapper::remove(const ObjectWrapper& object)
{
  if (!object.getNative())
  {
    std::string err;
    err.append("object ");
    err.append(object.get()->getName());
    err.append(" is not owned by this component");
    throw pybind11::attribute_error(err);
  }

  source_->remove(object.getNative());

  if (api_->getSyncCalls())
  {
    api_->commit();
  }
}

void ObjectSourceWrapper::definePythonApi(pybind11::module_& pythonModule)
{
  // our class
  pybind11::class_<ObjectSourceWrapper> api(pythonModule, "ObjectSource");
  api.def("add", &ObjectSourceWrapper::add, "Registers an object. Does nothing if already registered.");
  api.def("remove", &ObjectSourceWrapper::remove, "Unregisters an object. Does nothing if not present.");
}

}  // namespace sen::components::py
