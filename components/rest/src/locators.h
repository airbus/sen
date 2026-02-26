// === locators.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_LOCATORS_H
#define SEN_COMPONENTS_REST_SRC_LOCATORS_H

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/result.h"

// std
#include <string>

namespace sen::components::rest
{

enum class LocatorErrorType
{
  emptySessionName = 0,
  emptyBusName,
  emptyObjectName,
  emptyEventName,
  emptyPropertyName,
  emptyMethodName,
  emptyMemberName,
  parseSQLError
};

struct LocatorError
{
  LocatorErrorType errorType;
  std::string error;
};

/// Base class to specify the location of a Sen bus
class BusLocator
{
public:
  [[nodiscard]] static Result<BusLocator, LocatorError> build(std::string session, std::string bus) noexcept;
  [[nodiscard]] static Result<BusLocator, LocatorError> build(const Interest& interest) noexcept;

public:
  [[nodiscard]] std::string toBusAddress() const;
  [[nodiscard]] const std::string& session() const;
  [[nodiscard]] const std::string& bus() const;

protected:
  BusLocator(std::string session, std::string bus);

protected:
  void setSession(std::string session);
  void setBus(std::string bus);

private:
  std::string session_;
  std::string bus_;
};

/// Base class to specify the location of a Sen object
class ObjectLocator: public BusLocator
{
public:
  [[nodiscard]] static Result<ObjectLocator, LocatorError> build(const BusLocator& busLocator,
                                                                 const std::string& objectName) noexcept;

public:
  [[nodiscard]] std::string toObjectAddress() const;
  [[nodiscard]] const std::string& object() const;

protected:
  ObjectLocator(std::string session, std::string bus, std::string object);

protected:
  void setObject(std::string object);

private:
  std::string object_;
};

/// Base class to specify the location of a Sen object event
class EventLocator: public ObjectLocator
{
public:
  [[nodiscard]] static Result<EventLocator, LocatorError> build(const BusLocator& busLocator,
                                                                const std::string& objectName,
                                                                const std::string& eventName) noexcept;

public:
  [[nodiscard]] const std::string& event() const;

protected:
  EventLocator(std::string session, std::string bus, std::string object, std::string event);

protected:
  void setEvent(std::string event);

private:
  std::string event_;
};

/// Class to specify the location of a Sen object property
class PropertyLocator final: public ObjectLocator
{
public:
  [[nodiscard]] static Result<PropertyLocator, LocatorError> build(const BusLocator& busLocator,
                                                                   const std::string& objectName,
                                                                   const std::string& propertyName) noexcept;

public:
  [[nodiscard]] const std::string& property() const;

private:
  PropertyLocator(std::string session, std::string bus, std::string object, std::string property);

private:
  std::string property_;
};

/// Class to specify the location of a Sen object member
class MemberLocator final: public ObjectLocator
{
public:
  [[nodiscard]] static Result<MemberLocator, LocatorError> build(const BusLocator& busLocator,
                                                                 const std::string& objectName,
                                                                 const std::string& memberName) noexcept;

public:
  [[nodiscard]] const std::string& member() const;

private:
  MemberLocator(std::string session, std::string bus, std::string object, std::string member);

private:
  std::string member_;
};

/// Class to specify the location of a Sen object method
class MethodLocator final: public ObjectLocator
{
public:
  [[nodiscard]] static Result<MethodLocator, LocatorError> build(const BusLocator& busLocator,
                                                                 const std::string& objectName,
                                                                 const std::string& methodName) noexcept;

public:
  [[nodiscard]] const std::string& method() const;

private:
  MethodLocator(std::string session, std::string bus, std::string object, std::string method);

private:
  std::string method_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_LOCATORS_H
