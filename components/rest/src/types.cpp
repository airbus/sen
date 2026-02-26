// === types.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "types.h"

// component
#include "locators.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"

// std
#include <exception>
#include <string>

using Json = nlohmann::json;

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

inline std::string exceptionPtrToString(const std::exception_ptr& eptr)
{
  try
  {
    std::rethrow_exception(eptr);
    return "";
  }
  catch (const std::exception& e)
  {
    return e.what();
  }
}

//--------------------------------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------------------------------

std::string toJson(const Invoke& i)
{
  nlohmann::json j;
  switch (i.status)
  {
    case InvokeStatus::failed:
      j = Json {
        {"id", i.id.value_or(-1)},
        {"status", "failed"},
        {"error", exceptionPtrToString(i.result.getError())},
      };
      break;
    case InvokeStatus::finished:
      j = Json {
        {"id", i.id.value_or(-1)},
        {"status", "finished"},
        {"result", toJson(i.result.getValue())},
      };
      break;
    case InvokeStatus::pending:
      j = Json {
        {"id", i.id.value_or(-1)},
        {"status", "pending"},
      };
      break;
    default:
      j = Json {
        {"id", i.id.value_or(-1)},
        {"status", "unknown"},
      };
  }
  return j.dump();
}

std::string toJson(const sen::Object& object, const BusLocator& busLocator)
{
  auto objectClass = object.getClass();
  return Json {
    {"name", object.getName()},
    {"classname", objectClass->getQualifiedName()},
    {"localname", object.getLocalName()},
    {"session", busLocator.session()},
    {"bus", busLocator.bus()},
  }
    .dump();
}

std::string toJson(const sen::Object& object, const PropertyLocator& propertyLocator)
{
  const sen::Property* property = object.getClass()->searchPropertyByName(propertyLocator.property());
  if (!property)
  {
    ::sen::throwRuntimeError("Property not found");
  }

  auto objectClass = object.getClass();
  auto valueJsonString = toJson(object.getPropertyUntyped(property));
  return Json {
    {"name", object.getName()},
    {"classname", objectClass->getQualifiedName()},
    {"localname", object.getLocalName()},
    {"session", propertyLocator.session()},
    {"bus", propertyLocator.bus()},
    {"property", propertyLocator.property()},
    {"value", Json::parse(valueJsonString)},
  }
    .dump();
}

Interest fromJson(const Json& j)
{
  Interest interest;
  interest.query = j.at("query").get<std::string>();

  return interest;
}

}  // namespace sen::components::rest
