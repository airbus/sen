// === locators.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "locators.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/result.h"
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"

// std
#include <string>
#include <utility>

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Bus Locator
//--------------------------------------------------------------------------------------------------------------

BusLocator::BusLocator(std::string session, std::string bus): session_(std::move(session)), bus_(std::move(bus))
{
  SEN_ASSERT(!this->session_.empty());
  SEN_ASSERT(!this->bus_.empty());
}

Result<BusLocator, LocatorError> BusLocator::build(std::string session, std::string bus) noexcept
{
  if (session.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptySessionName, ""});
  }
  if (bus.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyBusName, ""});
  }

  return sen::Ok(BusLocator {std::move(session), std::move(bus)});
}

Result<BusLocator, LocatorError> BusLocator::build(const Interest& interest) noexcept
{
  sen::lang::StlScanner scanner(interest.query);
  const auto& tokens = scanner.scanTokens();

  try
  {
    lang::StlParser parser(tokens);
    auto busStatement = parser.parseQuery().bus;
    return BusLocator::build(busStatement.session.value().get<std::string>(),
                             busStatement.bus.value().get<std::string>());
  }
  catch (sen::lang::StlParser::ParseError& error)
  {
    return sen::Err(LocatorError {LocatorErrorType::parseSQLError, error.what()});
  }
}

std::string BusLocator::toBusAddress() const
{
  std::string handle = session_;
  handle.append(".");
  handle.append(bus_);

  return handle;
}

const std::string& BusLocator::session() const { return session_; }

const std::string& BusLocator::bus() const { return bus_; }

void BusLocator::setSession(std::string session) { session_ = std::move(session); }

void BusLocator::setBus(std::string bus) { bus_ = std::move(bus); }

//--------------------------------------------------------------------------------------------------------------
// Object Locator
//--------------------------------------------------------------------------------------------------------------

ObjectLocator::ObjectLocator(std::string session, std::string bus, std::string object)
  : BusLocator(std::move(session), std::move(bus)), object_(std::move(object))
{
  SEN_ASSERT(!this->object_.empty());
}

Result<ObjectLocator, LocatorError> ObjectLocator::build(const BusLocator& busLocator,
                                                         const std::string& objectName) noexcept
{
  if (objectName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyObjectName, ""});
  }
  return sen::Ok(ObjectLocator {busLocator.session(), busLocator.bus(), objectName});
}

std::string ObjectLocator::toObjectAddress() const
{
  std::string handle = toBusAddress();
  handle.append(".");
  handle.append(object_);

  return handle;
}

const std::string& ObjectLocator::object() const { return object_; }

void ObjectLocator::setObject(std::string object) { object_ = std::move(object); }

//--------------------------------------------------------------------------------------------------------------
// Event Locator
//--------------------------------------------------------------------------------------------------------------

EventLocator::EventLocator(std::string session, std::string bus, std::string object, std::string event)
  : ObjectLocator(std::move(session), std::move(bus), std::move(object)), event_(std::move(event))
{
  SEN_ASSERT(!this->event_.empty());
}

Result<EventLocator, LocatorError> EventLocator::build(const BusLocator& busLocator,
                                                       const std::string& objectName,
                                                       const std::string& eventName) noexcept
{
  if (objectName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyObjectName, ""});
  }
  if (eventName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyPropertyName, ""});
  }

  return sen::Ok(EventLocator {busLocator.session(), busLocator.bus(), objectName, eventName});
}

const std::string& EventLocator::event() const { return event_; }

void EventLocator::setEvent(std::string event) { event_ = std::move(event); }

//--------------------------------------------------------------------------------------------------------------
// Property Locator
//--------------------------------------------------------------------------------------------------------------

PropertyLocator::PropertyLocator(std::string session, std::string bus, std::string object, std::string property)
  : ObjectLocator(std::move(session), std::move(bus), std::move(object)), property_(std::move(property))
{
  SEN_ASSERT(!this->property_.empty());
}

Result<PropertyLocator, LocatorError> PropertyLocator::build(const BusLocator& busLocator,
                                                             const std::string& objectName,
                                                             const std::string& propertyName) noexcept
{
  if (objectName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyObjectName, ""});
  }
  if (propertyName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyPropertyName, ""});
  }

  return sen::Ok(PropertyLocator {busLocator.session(), busLocator.bus(), objectName, propertyName});
}

const std::string& PropertyLocator::property() const { return property_; }

//--------------------------------------------------------------------------------------------------------------
// Member Locator
//--------------------------------------------------------------------------------------------------------------

MemberLocator::MemberLocator(std::string session, std::string bus, std::string object, std::string member)
  : ObjectLocator(std::move(session), std::move(bus), std::move(object)), member_(std::move(member))
{
  SEN_ASSERT(!this->member_.empty());
}

Result<MemberLocator, LocatorError> MemberLocator::build(const BusLocator& busLocator,
                                                         const std::string& objectName,
                                                         const std::string& memberName) noexcept
{
  if (objectName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyObjectName, ""});
  }
  if (memberName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyMemberName, ""});
  }

  return sen::Ok(MemberLocator {busLocator.session(), busLocator.bus(), objectName, memberName});
}

const std::string& MemberLocator::member() const { return member_; }

//--------------------------------------------------------------------------------------------------------------
// Method Locator
//--------------------------------------------------------------------------------------------------------------

MethodLocator::MethodLocator(std::string session, std::string bus, std::string object, std::string method)
  : ObjectLocator(std::move(session), std::move(bus), std::move(object)), method_(std::move(method))
{
  SEN_ASSERT(!this->method_.empty());
}

Result<MethodLocator, LocatorError> MethodLocator::build(const BusLocator& busLocator,
                                                         const std::string& objectName,
                                                         const std::string& methodName) noexcept
{
  if (objectName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyObjectName, ""});
  }
  if (methodName.empty())
  {
    return sen::Err(LocatorError {LocatorErrorType::emptyMethodName, ""});
  }

  return sen::Ok(MethodLocator {busLocator.session(), busLocator.bus(), objectName, methodName});
}

const std::string& MethodLocator::method() const { return method_; }

}  // namespace sen::components::rest
