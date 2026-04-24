// === object_wrapper.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_wrapper.h"

// component
#include "component_api.h"
#include "variant_conversion.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"

// pybind11
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

// python
#include <object.h>

// std
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <regex>
#include <string>
#include <utility>

namespace sen::components::py
{

namespace
{

// -------------------------------------------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::string& unCapitalize(std::string& str) noexcept
{
  str[0] = static_cast<std::string::value_type>(std::tolower(str[0]));
  return str;
}

}  // namespace

// -------------------------------------------------------------------------------------------------------------
// ObjectWrapper
// -------------------------------------------------------------------------------------------------------------

ObjectWrapper ObjectWrapper::makeGeneric(std::shared_ptr<Object> object, RunApi* api) { return {object, api}; }

ObjectWrapper ObjectWrapper::makeNative(std::shared_ptr<NativeObject> object, RunApi* api) { return {object, api}; }

ObjectWrapper::ObjectWrapper(std::shared_ptr<Object> obj, RunApi* api)
  : obj_(std::move(obj)), workQueue_(api->getWorkQueue()), api_(api)
{
}

ObjectWrapper::ObjectWrapper(std::shared_ptr<NativeObject> obj, RunApi* api)
  : obj_(obj), native_(obj), workQueue_(api->getWorkQueue()), api_(api)
{
}

std::shared_ptr<Object> ObjectWrapper::get() const noexcept { return obj_; }

std::shared_ptr<NativeObject> ObjectWrapper::getNative() const noexcept { return native_; }

std::string ObjectWrapper::str() const
{
  std::string result;
  result.append("<");
  result.append(obj_->getClass()->getQualifiedName());
  result.append(" named '");
  result.append(obj_->getName());
  result.append("'>");
  return result;
}

uint32_t ObjectWrapper::hash() const { return obj_->getId().get(); }

void ObjectWrapper::definePythonApi(pybind11::module_& args)
{
  pybind11::class_<ObjectWrapper> theClass(args, "ObjectWrapper");
  theClass.def("__str__", &ObjectWrapper::str);
  theClass.def("__hash__", &ObjectWrapper::hash);
  theClass.def("__getattribute__", &ObjectWrapper::getAttribute, pybind11::return_value_policy::take_ownership);
  theClass.def("__setattr__", &ObjectWrapper::setAttribute);
}

pybind11::object ObjectWrapper::getAttribute(const std::string& name)
{
  if (name == "name")
  {
    return pybind11::cast(obj_->getName());
  }
  if (name == "localName")
  {
    return pybind11::cast(obj_->getLocalName());
  }
  if (name == "id")
  {
    return pybind11::cast(obj_->getId().get());
  }
  if (name == "lastCommitTime")
  {
    return pybind11::cast(obj_->getLastCommitTime().sinceEpoch().toChrono());
  }
  if (name == "className")
  {
    return pybind11::cast(obj_->getClass()->getQualifiedName());
  }

  const auto* meta = obj_->getClass().type();

  // check if we are accessing a property getter (setters are handled by the __setattr__ mechanism).
  if (const auto* prop = meta->searchPropertyByName(name))
  {
    return toPython(obj_->getPropertyUntyped(prop));
  }

  // check if we are calling a method, and return a function that can be called by the python code
  if (const auto* method = meta->searchMethodByName(name))
  {
    return pybind11::cpp_function([this, method](const pybind11::args& args) { return callMethod(method, args); });
  }

  // check if we are installing a property change callback
  if (const auto* prop = searchPropertyChange(name))
  {
    return pybind11::cpp_function([this, prop](const pybind11::args& args) { installChangeCallback(prop, args); });
  }

  // check if we are installing an event callback
  if (const auto* ev = searchEvent(name))
  {
    return pybind11::cpp_function([this, ev](const pybind11::args& args) { installEventCallback(ev, args); });
  }

  std::string err;
  err.append("invalid member '");
  err.append(name);
  err.append("' for object ");
  err.append(obj_->getName());
  err.append(" of class ");
  err.append(obj_->getClass()->getQualifiedName());
  getLogger()->error(err);
  throw pybind11::attribute_error(err);
}

void ObjectWrapper::installChangeCallback(const Property* prop, const pybind11::args& args)
{
  if (args.size() != 1)
  {
    std::string err;
    err.append("expecting a callback as an argument for reacting to changes of the property ");
    err.append(prop->getName());
    err.append(" in the object ");
    err.append(obj_->getName());
    getLogger()->error(err);
    throw pybind11::value_error(err);
  }

  const auto* holderPtr = new pybind11::args(args);  // NOLINT
  holderPtr->inc_ref();

  obj_
    ->onPropertyChangedUntyped(prop,
                               {workQueue_,
                                [holderPtr](const auto& /*result*/)
                                {
                                  pybind11::gil_scoped_acquire acquire;
                                  ((*holderPtr)[0])();
                                }})
    .keep();
}

void ObjectWrapper::installEventCallback(const Event* event, const pybind11::args& args)
{
  if (args.size() != 1)
  {
    std::string err;
    err.append("expecting a callback as an argument for reacting the event ");
    err.append(event->getName());
    err.append(" in the object ");
    err.append(obj_->getName());
    getLogger()->error(err);
    throw pybind11::value_error(err);
  }

  const auto* holderPtr = new pybind11::args(args);  // NOLINT
  holderPtr->inc_ref();

  obj_
    ->onEventUntyped(event,
                     {workQueue_,
                      [holderPtr](const auto& result) mutable
                      {
                        pybind11::gil_scoped_acquire acquire;
                        ((*holderPtr)[0])(toPython(result));
                      }})
    .keep();
}

pybind11::object ObjectWrapper::callMethod(const Method* method, const pybind11::args& args)
{
  const auto* meta = obj_->getClass().type();
  const auto& metaArgs = method->getArgs();
  if (args.size() < metaArgs.size())
  {
    std::string err;
    err.append("method named ");
    err.append(method->getName());
    err.append(" of class ");
    err.append(meta->getQualifiedName().data());
    err.append(" expects a minimum of ");
    err.append(std::to_string(metaArgs.size()));
    err.append(" arguments, but got ");
    err.append(std::to_string(args.size()));
    getLogger()->error(err);
    throw pybind11::attribute_error(err);
  }

  // convert the args to Sen variants, checking the type
  VarList varList;
  varList.reserve(metaArgs.size());
  for (std::size_t i = 0; i < metaArgs.size(); ++i)
  {
    varList.push_back(fromPython(args[i], *metaArgs[i].type));
  }

  if (api_->getSyncCalls())
  {
    std::atomic_bool finishedCall = false;
    bool success = false;
    std::string errorString;
    pybind11::object result = pybind11::none();

    obj_->invokeUntyped(method,
                        varList,
                        {workQueue_,
                         [&errorString, &success, method, &finishedCall, &result](const MethodResult<Var>& var)
                         {
                           if (var.isError())
                           {
                             try
                             {
                               std::rethrow_exception(var.getError());
                             }
                             catch (const std::exception& err)
                             {
                               errorString = err.what();
                             }
                             catch (...)
                             {
                               errorString = "unknown exception";
                             }
                             success = false;
                           }
                           else
                           {
                             if (!method->getReturnType()->isVoidType())
                             {
                               result = toPython(var.getValue());
                             }

                             success = true;
                           }
                           finishedCall = true;
                         }});

    if (auto calledIt = api_->waitUntilCpp([&finishedCall]() { return finishedCall.load(); }); !calledIt)
    {
      throw pybind11::attribute_error("call time out");
    }

    if (!success)
    {
      throw pybind11::attribute_error(errorString);
    }

    return result;
  }

  // check if the user provides a callback for handling the result
  if (args.size() == metaArgs.size() + 1 && pybind11::isinstance<pybind11::function>(args[args.size() - 1]))
  {
    const auto* holderPtr = new pybind11::args(args);  // NOLINT
    holderPtr->inc_ref();

    obj_->invokeUntyped(method,
                        varList,
                        {workQueue_,
                         [this, method, holderPtr](const auto& result)
                         {
                           if (result.isOk())
                           {
                             try
                             {
                               pybind11::gil_scoped_acquire acquire;
                               if (method->getReturnType()->isVoidType())
                               {
                                 ((*holderPtr)[holderPtr->size() - 1])();
                               }
                               else
                               {
                                 ((*holderPtr)[holderPtr->size() - 1])(toPython(result.getValue()));
                               }
                             }
                             catch (pybind11::error_already_set& err)
                             {
                               getLogger()->error("error detected in Python callback: {}", err.what());
                               err.discard_as_unraisable(__func__);  // NOLINT
                               ((*holderPtr)[holderPtr->size() - 1])(pybind11::cast<pybind11::none>(Py_None));
                             }
                             catch (const std::exception& err)
                             {
                               getLogger()->error("unknown error detected in Python callback: {}", err.what());
                             }
                           }
                           else
                           {
                             try
                             {
                               std::rethrow_exception(result.getError());
                             }
                             catch (const std::exception& err)
                             {
                               getLogger()->error(
                                 "Exception raised while calling the method {} on object {} from Python: {}",
                                 method->getName(),
                                 obj_->getName(),
                                 err.what());
                             }
                           }
                         }});
  }
  else
  {
    obj_->invokeUntyped(method, varList);
  }

  return pybind11::none();
}

void ObjectWrapper::setAttribute(const std::string& name, pybind11::object value)
{
  const auto* meta = obj_->getClass().type();
  const auto* prop = meta->searchPropertyByName(name);
  if (prop == nullptr)
  {
    std::string err;
    err.append("no property named ");
    err.append(name);
    err.append(" found in class ");
    err.append(meta->getQualifiedName().data());
    getLogger()->error(err);
    throw pybind11::attribute_error(err);
  }

  if (prop->getCategory() == PropertyCategory::dynamicRO || prop->getCategory() == PropertyCategory::staticRW)
  {
    std::string err;
    err.append("property ");
    err.append(name);
    err.append(" of class ");
    err.append(meta->getQualifiedName().data());
    err.append(" is not settable");
    getLogger()->error(err);
    throw pybind11::attribute_error(err);
  }

  VarList varList;
  varList.reserve(1U);
  varList.push_back(fromPython(value, *prop->getType()));

  obj_->invokeUntyped(&prop->getSetterMethod(),
                      varList,
                      {workQueue_,
                       [prop](const auto& result)
                       {
                         if (!result.isOk())
                         {
                           getLogger()->error("could not set property {} from Python", prop->getName());
                         }
                       }});
}

const Property* ObjectWrapper::searchPropertyChange(const std::string& name) const
{
  static std::regex expression("on([a-zA-Z0-9_]+)Changed");
  if (std::smatch match; std::regex_search(name, match, expression) && match.size() >= 2)
  {
    std::string propName = match[1].str();
    if (!propName.empty())
    {
      return obj_->getClass()->searchPropertyByName(unCapitalize(propName));
    }
  }

  return nullptr;
}

const Event* ObjectWrapper::searchEvent(const std::string& name) const
{
  static std::regex expression("on([a-zA-Z0-9_]+)");
  if (std::smatch match; std::regex_search(name, match, expression) && match.size() >= 2)
  {
    std::string eventName = match[1].str();
    if (!eventName.empty())
    {
      return obj_->getClass()->searchEventByName(unCapitalize(eventName));
    }
  }

  return nullptr;
}

}  // namespace sen::components::py
