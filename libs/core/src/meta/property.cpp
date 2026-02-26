// === property.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/property.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen
{
namespace
{

/// Helper to explain an error by adding the property name to a message
std::string makeExplanation(const PropertySpec& spec, std::string_view message)
{
  std::string result = "error in property '";
  result += spec.name;
  result += "' spec: ";
  result += message;

  return result;
}

/// Helper that throws a property-related error
[[noreturn]] void throwError(const PropertySpec& spec, const std::string& message)
{
  sen::throwRuntimeError(makeExplanation(spec, message));
}

/// Throws if the property does not have all the required fields
void checkRequiredFields(const PropertySpec& spec) { impl::checkMemberName(spec.name); }

/// Throws if the getter method is not valid
void checkGetterMethod(const PropertySpec& spec, const Method& getter)
{
  // getters shall have no arguments
  if (!getter.getArgs().empty())
  {
    throwError(spec, "getter method takes arguments");
  }

  // getters shall be constant
  if (getter.getConstness() != Constness::constant)
  {
    throwError(spec, "getter method is not constant");
  }

  // getters shall return something
  if (getter.getReturnType()->isVoidType())
  {
    throwError(spec, "getter method does not return anything");
  }

  // getters shall return the same type as the property
  if (getter.getReturnType() != spec.type)
  {
    throwError(spec, "getter method returns a different type");
  }

  // getters naming shall be consistent with our expectations
  if (getter.getName() != Property::makeGetterMethodName(spec))
  {
    throwError(spec, "getter method has an invalid name");
  }
}

/// Throws if the setter method is not valid
void checkSetterMethod(const PropertySpec& spec, const Method& setter)
{
  // setters shall have only one argument
  if (setter.getArgs().size() != 1U)
  {
    throwError(spec, "setter does not have one argument");
  }

  // setters shall be non-constant
  if (setter.getConstness() == Constness::constant)
  {
    throwError(spec, "setter method is constant");
  }

  // setters shall not return
  if (!setter.getReturnType()->isVoidType())
  {
    throwError(spec, "setter method returns something");
  }

  // setters shall take the same type as the property
  if (setter.getArgs()[0U].type != spec.type)
  {
    throwError(spec, "setter method takes a different type");
  }

  // setters naming shall be consistent with our expectations
  if (setter.getName() != Property::makeSetterMethodName(spec))
  {
    throwError(spec, "setter method has an invalid name");
  }
}

/// Throws if the change notification event is not valid
void checkChangeNotificationEvent(const PropertySpec& spec, const Event& changeEvent)
{
  // change events shall have no arguments
  if (!changeEvent.getArgs().empty())
  {
    throwError(spec, "change event has arguments");
  }

  // event naming shall be consistent with our expectations
  if (changeEvent.getName() != Property::makeChangeNotificationEventName(spec))
  {
    throwError(spec, "change event has an invalid name");
  }
}

}  // namespace

Property::Property(PropertySpec spec, Private /*priv*/)
  : spec_(std::move(spec)), id_ {hashCombine(propertyHashSeed, spec_.name)}, hash_(impl::hash<PropertySpec>()(spec_))
{
  {
    auto getterSpecConstness = Constness::constant;
    auto getterSpecReturnType = spec_.type;

    CallableSpec getterCallableSpec;
    getterCallableSpec.name = makeGetterMethodName(spec_);
    getterCallableSpec.description = std::string("getter for property ") + spec_.name;
    getterCallableSpec.transportMode = TransportMode::confirmed;

    auto getterSpecPropertyRelation = PropertyGetter {this};

    MethodSpec getterSpec(getterCallableSpec, getterSpecReturnType, getterSpecConstness, getterSpecPropertyRelation);
    getterMethod_ = Method::make(getterSpec);
  }

  {
    auto setterSpecConstness = Constness::nonConstant;
    auto setterSpecReturnType = sen::VoidType::get();

    CallableSpec setterCallableSpec;
    setterCallableSpec.name = makeSetterMethodName(spec_);
    setterCallableSpec.description = std::string("setter for property ") + spec_.name;
    setterCallableSpec.args.push_back(Arg {spec_.name, {}, spec_.type});  // TODO: fix
    setterCallableSpec.transportMode = TransportMode::confirmed;

    auto setterSpecPropertyRelation = PropertySetter {this};

    MethodSpec setterSpec(setterCallableSpec, setterSpecReturnType, setterSpecConstness, setterSpecPropertyRelation);

    setterMethod_ = Method::make(setterSpec);
  }

  {
    EventSpec changeEventSpec {};
    changeEventSpec.callableSpec.name = makeChangeNotificationEventName(spec_);
    changeEventSpec.callableSpec.description = std::string("change notification for property ") + spec_.name;
    changeEventSpec.callableSpec.transportMode = spec_.transportMode;
    changeEvent_ = Event::make(changeEventSpec);
  }

  // just to be safe
  checkGetterMethod(spec_, *getterMethod_);            // check the getter method
  checkSetterMethod(spec_, *setterMethod_);            // check the setter method
  checkChangeNotificationEvent(spec_, *changeEvent_);  // check the event
}

std::shared_ptr<Property> Property::make(PropertySpec spec)
{
  // we need to validate the spec and ensure that the methods
  // and the event match our expectations regarding types

  checkRequiredFields(spec);  // first check that we have all we need

  // if we get here, no problems were found
  return std::make_shared<Property>(std::move(spec), Private {});
}

std::string_view Property::getName() const noexcept { return spec_.name; }

std::string_view Property::getDescription() const noexcept { return spec_.description; }

MemberHash Property::getId() const noexcept { return id_; }

bool Property::getCheckedSet() const noexcept { return spec_.checkedSet; }

PropertyCategory Property::getCategory() const noexcept { return spec_.category; }

ConstTypeHandle<> Property::getType() const noexcept { return spec_.type; }

TransportMode Property::getTransportMode() const noexcept { return spec_.transportMode; }

const Method& Property::getGetterMethod() const noexcept { return *getterMethod_; }

const Method& Property::getSetterMethod() const noexcept { return *setterMethod_; }

const Event& Property::getChangeEvent() const noexcept { return *changeEvent_; }

const std::vector<std::string>& Property::getTags() const noexcept { return spec_.tags; }

bool Property::operator==(const Property& other) const noexcept { return &other == this || spec_ == other.spec_; }

bool Property::operator!=(const Property& other) const noexcept { return !(*this == other); }

MemberHash Property::getHash() const noexcept { return hash_; }

std::string Property::makeGetterMethodName(const PropertySpec& spec)
{
  impl::checkMemberName(spec.name);
  return std::string("get") + impl::capitalize(spec.name);
}

std::string Property::makeSetterMethodName(const PropertySpec& spec)
{
  impl::checkMemberName(spec.name);
  return std::string("setNext") + impl::capitalize(spec.name);
}

std::string Property::makeChangeNotificationEventName(const PropertySpec& spec)
{
  impl::checkMemberName(spec.name);
  return spec.name + std::string("Changed");
}

}  // namespace sen
