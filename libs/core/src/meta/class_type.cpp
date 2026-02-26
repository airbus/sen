// === class_type.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/class_type.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/strong_type.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Helper to explain an error by adding the property name to a message
std::string makeExplanation(const ClassSpec& spec, std::string_view message)
{
  std::string result = "error in class '";
  result += spec.name;
  result += "' spec: ";
  result += message;

  return result;
}

/// Helper that throws a property-related error
[[noreturn]] void throwError(const ClassSpec& spec, const std::string& message)
{
  sen::throwRuntimeError(makeExplanation(spec, message));
}

/// Throws if the class type does not have all the required fields
void checkRequiredFields(const ClassSpec& spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);
}

void checkLocalProperties(const ClassSpec& spec)
{
  const auto propertyCount = spec.properties.size();

  // all properties shall have unique names and unique keys
  for (std::size_t i = 0U; i < propertyCount; i++)
  {
    const auto& currentProperty = spec.properties.at(i);

    for (std::size_t j = 0U; j < propertyCount; j++)
    {
      if (j != i)
      {
        const auto& otherProperty = spec.properties.at(j);

        // check the name
        if (otherProperty.name == currentProperty.name)
        {
          std::string err;
          err.append("duplicated property name '");
          err.append(currentProperty.name);
          err.append("'");
          throwError(spec, err);
        }
      }
    }
  }
}

void checkInheritedProperties(const ClassSpec& spec)
{
  // properties must have unique names in the hierarchy
  for (const auto& prop: spec.properties)
  {
    for (const auto& parent: spec.parents)
    {
      if (parent->searchPropertyByName(prop.name))
      {
        std::string err;
        err.append("parent class ");
        err.append(parent->getQualifiedName());
        err.append(" already has a property named ");
        err.append(prop.name);
        throwError(spec, err);
      }
    }
  }
}

void checkAndBuildProperties(const ClassSpec& spec,
                             PropertyList& properties,
                             std::unordered_map<MemberHash, Property*>& idToPropertyMap,
                             std::unordered_map<std::string, Property*>& nameToPropertyMap)
{
  checkLocalProperties(spec);
  checkInheritedProperties(spec);

  // create the properties
  const auto propSize = spec.properties.size();
  properties.reserve(propSize);
  idToPropertyMap.reserve(propSize);
  nameToPropertyMap.reserve(propSize);
  for (const auto& prop: spec.properties)
  {
    auto metaProp = Property::make(prop);
    idToPropertyMap.emplace(metaProp->getId(), metaProp.get());
    nameToPropertyMap.emplace(metaProp->getName(), metaProp.get());
    properties.push_back(std::move(metaProp));
  }
}

void checkLocalMethods(const ClassSpec& spec)
{
  const auto methodCount = spec.methods.size();

  // all methods shall have unique names and id
  for (std::size_t i = 0U; i < methodCount; i++)
  {
    const auto& currentMethod = spec.methods.at(i);

    for (std::size_t j = 0U; j < methodCount; j++)
    {
      if (j != i)
      {
        const auto& otherMethod = spec.methods.at(j);

        // check the name
        if (otherMethod.callableSpec.name == currentMethod.callableSpec.name)
        {
          std::string err;
          err.append("method with repeated name '");
          err.append(otherMethod.callableSpec.name);
          err.append("'");
          throwError(spec, err);
        }
      }
    }
  }
}

void checkInheritedMethods(const ClassSpec& spec)
{
  // properties must have unique names in the hierarchy
  for (const auto& method: spec.methods)
  {
    for (const auto& parent: spec.parents)
    {
      if (parent->searchMethodByName(method.callableSpec.name))
      {
        std::string err;
        err.append("parent class ");
        err.append(parent->getQualifiedName());
        err.append(" already has a method named ");
        err.append(method.callableSpec.name);
        throwError(spec, err);
      }
    }
  }
}

void checkAndBuildMethods(const ClassSpec& spec,
                          MethodList& methods,
                          std::unordered_map<MemberHash, Method*>& idToMethodMap,
                          std::unordered_map<std::string, Method*>& nameToMethodMap)
{
  checkLocalMethods(spec);
  checkInheritedMethods(spec);

  // create the methods
  const auto methodsSize = spec.methods.size();
  methods.reserve(methodsSize);
  idToMethodMap.reserve(methodsSize);
  nameToMethodMap.reserve(methodsSize);
  for (const auto& method: spec.methods)
  {
    auto newMethod = Method::make(method);
    idToMethodMap.emplace(newMethod->getId(), newMethod.get());
    nameToMethodMap.emplace(newMethod->getName(), newMethod.get());
    methods.push_back(std::move(newMethod));
  }
}

void checkLocalEvents(const ClassSpec& spec)
{
  const auto eventCount = spec.events.size();

  // all events shall have unique names and ids
  for (std::size_t i = 0U; i < eventCount; i++)
  {
    const auto& currentEvent = spec.events.at(i);

    for (std::size_t j = 0U; j < spec.events.size(); j++)
    {
      if (j != i)
      {
        const auto& otherEvent = spec.events.at(j);

        // check the name
        if (otherEvent.callableSpec.name == currentEvent.callableSpec.name)
        {
          std::string err;
          err.append("event with repeated name '");
          err.append(otherEvent.callableSpec.name);
          err.append("'");
          throwError(spec, err);
        }
      }
    }
  }
}

void checkInheritedEvents(const ClassSpec& spec)
{
  // properties must have unique names in the hierarchy
  for (const auto& event: spec.events)
  {
    for (const auto& parent: spec.parents)
    {
      if (parent->searchEventByName(event.callableSpec.name))
      {
        std::string err;
        err.append("parent class ");
        err.append(parent->getQualifiedName());
        err.append(" already has an event named ");
        err.append(event.callableSpec.name);
        throwError(spec, err);
      }
    }
  }
}

void checkAndBuildEvents(const ClassSpec& spec,
                         EventList& events,
                         std::unordered_map<MemberHash, Event*>& idToEventMap,
                         std::unordered_map<std::string, Event*>& nameToEventMap)
{
  checkLocalEvents(spec);
  checkInheritedEvents(spec);

  // create the events
  const auto eventSize = spec.events.size();
  events.reserve(eventSize);
  idToEventMap.reserve(eventSize);
  nameToEventMap.reserve(eventSize);
  for (const auto& event: spec.events)
  {
    auto metaEvent = Event::make(event);
    idToEventMap.emplace(metaEvent->getId(), metaEvent.get());
    nameToEventMap.emplace(metaEvent->getName(), metaEvent.get());
    events.push_back(std::move(metaEvent));
  }
}

void checkParents(const ClassSpec& spec)
{
  if (spec.isInterface && !spec.parents.empty())
  {
    throwError(spec, "interface classes cannot have parents");
  }

  const auto parentCount = spec.parents.size();

  // parents must be unique and not inherit from another
  for (std::size_t i = 0U; i < parentCount; i++)
  {
    const auto& currentParent = spec.parents.at(i);

    for (std::size_t j = 0U; j < parentCount; j++)
    {
      if (j != i)
      {
        const auto& otherParent = spec.parents.at(j);

        // check the name
        if (currentParent->isSameOrInheritsFrom(*otherParent))
        {
          std::string err;
          err.append("invalid parent class '");
          err.append(otherParent->getQualifiedName());
          err.append("' check the hierarchy as it might be repeated.");
          throwError(spec, err);
        }
      }
    }
  }
}

void checkAndBuildConstructor(const ClassSpec& spec, std::shared_ptr<const Method>& method)
{
  if (!spec.constructor.has_value())
  {
    return;
  }

  method = Method::make(spec.constructor.value());
}

[[nodiscard]] std::string intToHex(uint32_t i)
{
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(sizeof(uint32_t) + sizeof(uint32_t)) << std::hex << i;
  return stream.str();
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// ClassType
//--------------------------------------------------------------------------------------------------------------

ClassType::ClassType(ClassSpec spec, Private notUsable)
  : CustomType(impl::hash<ClassSpec>()(spec)), spec_(std::move(spec)), id_(hashCombine(hashSeed, spec_.qualifiedName))
{
  std::ignore = notUsable;
}

TypeHandle<ClassType> ClassType::make(const ClassSpec& spec)
{
  checkRequiredFields(spec);

  // create the class and populate its members
  // while checking each of them
  TypeHandle<ClassType> result(spec, Private {});

  checkAndBuildProperties(spec, result->properties_, result->idToPropertyMap_, result->nameToPropertyMap_);
  checkAndBuildMethods(spec, result->methods_, result->idToMethodMap_, result->nameToMethodMap_);
  checkAndBuildEvents(spec, result->events_, result->idToEventMap_, result->nameToEventMap_);
  checkParents(spec);
  checkAndBuildConstructor(spec, result->constructor_);

  return result;
}

TypeHandle<ClassType> ClassType::makeImplementation(std::string_view name,
                                                    ConstTypeHandle<ClassType> parent,
                                                    const char* packagePath)
{
  // compute the qualified name of the child class
  std::string qualifiedName;
  if (packagePath)
  {
    qualifiedName = packagePath;
    qualifiedName.append(".");
    qualifiedName.append(name);
  }
  else
  {
    const std::string fullParentName(parent->getQualifiedName());
    qualifiedName = fullParentName.substr(0, fullParentName.find_last_of('.'));
    qualifiedName.append(".");
    qualifiedName.append(name);
  }

  // build the specification
  ClassSpec spec {};
  spec.name = name;
  spec.qualifiedName = qualifiedName;
  spec.description = std::string("Implementation of ") + std::string(parent->getQualifiedName());
  spec.parents.push_back(parent);
  spec.isInterface = false;
  spec.constructor = parent->spec_.constructor;
  spec.remoteProxyMaker = parent->spec_.remoteProxyMaker;
  spec.localProxyMaker = parent->spec_.localProxyMaker;

  return make(spec);
}

const Property* ClassType::searchPropertyByName(std::string_view name, SearchMode mode) const
{
  // search in this class
  for (const auto& prop: properties_)
  {
    if (prop->getName() == name)
    {
      return prop.get();
    }
  }

  // search in our parents
  if (mode == SearchMode::includeParents)
  {
    for (const auto& parent: spec_.parents)
    {
      if (const auto* prop = parent->searchPropertyByName(name); prop)
      {
        return prop;
      }
    }
  }

  // if we get here, nothing was found
  return nullptr;
}

const Property* ClassType::searchPropertyById(MemberHash id, SearchMode mode) const
{
  if (const auto& itr = idToPropertyMap_.find(id); itr != idToPropertyMap_.end())
  {
    return itr->second;
  }

  // search in our parents
  if (mode == SearchMode::includeParents)
  {
    for (const auto& parent: spec_.parents)
    {
      if (auto* prop = parent->searchPropertyById(id); prop)
      {
        return prop;
      }
    }
  }

  // if we get here, nothing was found
  return nullptr;
}

const Method* ClassType::searchMethodById(MemberHash id, SearchMode mode) const
{
  if (const auto& itr = idToMethodMap_.find(id); itr != idToMethodMap_.end())
  {
    return itr->second;
  }

  // search in the properties
  for (const auto& prop: properties_)
  {
    if (const auto* getter = &prop->getGetterMethod(); getter->getId() == id)
    {
      return getter;
    }

    if (prop->getCategory() == PropertyCategory::dynamicRW && prop->getSetterMethod().getId() == id)
    {
      return &prop->getSetterMethod();
    }
  }

  // search in our parents
  if (mode == SearchMode::includeParents)
  {
    for (const auto& parent: spec_.parents)
    {
      if (const auto* method = parent->searchMethodById(id); method)
      {
        return method;
      }
    }
  }

  // if we get here, nothing was found
  return nullptr;
}

const Event* ClassType::searchEventById(MemberHash id, SearchMode mode) const
{
  if (const auto& itr = idToEventMap_.find(id); itr != idToEventMap_.end())
  {
    return itr->second;
  }

  // search in our parents
  if (mode == SearchMode::includeParents)
  {
    for (const auto& parent: spec_.parents)
    {
      if (const auto* ev = parent->searchEventById(id); ev)
      {
        return ev;
      }
    }
  }

  // if we get here, nothing was found
  return nullptr;
}

const Method* ClassType::searchMethodByName(std::string_view name, SearchMode mode) const
{
  // search in this class
  for (const auto& method: methods_)
  {
    if (method->getName() == name)
    {
      return method.get();
    }
  }

  // search in the properties
  for (const auto& prop: properties_)
  {
    if (const auto* getter = &prop->getGetterMethod(); getter->getName() == name)
    {
      return getter;
    }

    if (prop->getCategory() == PropertyCategory::dynamicRW && prop->getSetterMethod().getName() == name)
    {
      return &prop->getSetterMethod();
    }
  }

  // search in our parents
  if (mode == SearchMode::includeParents)
  {
    for (const auto& parent: spec_.parents)
    {
      if (const auto* method = parent->searchMethodByName(name); method)
      {
        return method;
      }
    }
  }

  // if we get here, nothing was found
  return nullptr;
}

const Event* ClassType::searchEventByName(std::string_view name, SearchMode mode) const
{
  // search in this class
  for (const auto& event: events_)
  {
    if (event->getName() == name)
    {
      return event.get();
    }
  }

  // search in the properties
  for (const auto& prop: properties_)
  {
    if (const auto* changeEvent = &prop->getChangeEvent(); changeEvent->getName() == name)
    {
      return changeEvent;
    }
  }

  // search in our parents
  if (mode == SearchMode::includeParents)
  {
    for (const auto& parent: spec_.parents)
    {
      if (const auto* event = parent->searchEventByName(name); event)
      {
        return event;
      }
    }
  }

  // if we get here, nothing was found
  return nullptr;
}

bool ClassType::isSameOrInheritsFrom(const ClassType& other) const noexcept
{
  if (getHash() == other.getHash())
  {
    return true;
  }

  // search in our parents
  return std::any_of(
    spec_.parents.begin(), spec_.parents.end(), [&](const auto& elem) { return elem->isSameOrInheritsFrom(other); });
}

bool ClassType::isSameOrInheritsFrom(const std::string& otherClassName) const noexcept
{
  if (getQualifiedName() == otherClassName)
  {
    return true;
  }

  // search in our parents
  return std::any_of(spec_.parents.begin(),
                     spec_.parents.end(),
                     [&](const auto& elem) { return elem->isSameOrInheritsFrom(otherClassName); });
}

MemberHash ClassType::computeMethodHash(MemberHash classId, const std::string& name)
{
  static const auto methodSeed = crc32("methods");
  return hashCombine(methodSeed, classId, crc32(name));
}

MemberHash ClassType::computeEventHash(MemberHash classId, const std::string& name)
{
  static const auto eventsSeed = crc32("events");
  return hashCombine(eventsSeed, classId, crc32(name));
}

std::string ClassType::computeTypeGetterFuncName(const std::string& qualName)
{
  auto typeIdHex = intToHex(crc32(qualName));

  std::string result = "senGetType";
  result.append(typeIdHex);
  return result;
}

std::string ClassType::computeInstanceMakerFuncName(const std::string& qualName)
{
  auto typeIdHex = intToHex(crc32(qualName));

  std::string result = "senMakeInstance";
  result.append(typeIdHex);
  return result;
}

bool operator==(const ClassSpec& lhs, const ClassSpec& rhs) noexcept
{
  if (&lhs == &rhs)
  {
    return true;
  }

  if (lhs.name != rhs.name || lhs.qualifiedName != rhs.qualifiedName || lhs.description != rhs.description ||
      lhs.isInterface != rhs.isInterface || lhs.properties != rhs.properties || lhs.methods != rhs.methods ||
      lhs.events != rhs.events || lhs.constructor != rhs.constructor || lhs.parents.size() != rhs.parents.size())
  {
    return false;
  }

  for (std::size_t i = 0U; i < lhs.parents.size(); ++i)
  {
    if (*lhs.parents.at(i) != *rhs.parents.at(i))
    {
      return false;
    }
  }

  return true;
}

bool operator!=(const ClassSpec& lhs, const ClassSpec& rhs) noexcept { return !(lhs == rhs); }

PropertyList ClassType::getProperties(SearchMode mode) const
{
  switch (mode)
  {
    case SearchMode::doNotIncludeParents:
    {
      return properties_;
    }

    case SearchMode::includeParents:
    {
      PropertyList list;

      for (const auto& parent: spec_.parents)
      {
        auto parentProps = parent->getProperties(mode);
        list.insert(list.end(), parentProps.begin(), parentProps.end());
      }

      list.insert(list.end(), properties_.begin(), properties_.end());
      return list;
    }

    default:
      SEN_UNREACHABLE();
  }
}

MethodList ClassType::getMethods(SearchMode mode) const
{
  switch (mode)
  {
    case SearchMode::doNotIncludeParents:
    {
      return methods_;
    }

    case SearchMode::includeParents:
    {
      MethodList list;

      for (const auto& parent: spec_.parents)
      {
        auto parentMethods = parent->getMethods(mode);
        list.insert(list.end(), parentMethods.begin(), parentMethods.end());
      }

      list.insert(list.end(), methods_.begin(), methods_.end());
      return list;
    }

    default:
      SEN_UNREACHABLE();
  }
}

EventList ClassType::getEvents(SearchMode mode) const
{
  switch (mode)
  {
    case SearchMode::doNotIncludeParents:
    {
      return events_;
    }

    case SearchMode::includeParents:
    {
      EventList list;

      for (const auto& parent: spec_.parents)
      {
        auto parentEvents = parent->getEvents(mode);
        list.insert(list.end(), parentEvents.begin(), parentEvents.end());
      }

      list.insert(list.end(), events_.begin(), events_.end());
      return list;
    }

    default:
      SEN_UNREACHABLE();
  }
}

const ClassList& ClassType::getParents() const noexcept { return spec_.parents; }

const Method* ClassType::getConstructor() const noexcept { return constructor_.get(); }

std::string_view ClassType::getName() const noexcept { return spec_.name; }

std::string_view ClassType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view ClassType::getDescription() const noexcept { return spec_.description; }

bool ClassType::isInterface() const noexcept { return spec_.isInterface; }

bool ClassType::isAbstract() const noexcept { return spec_.isAbstract; }

MemberHash ClassType::getId() const noexcept { return id_; }

bool ClassType::isBounded() const noexcept { return false; }

std::shared_ptr<impl::RemoteObject> ClassType::createRemoteProxy(impl::RemoteObjectInfo&& info) const
{
  return spec_.remoteProxyMaker(std::move(info));
}

void ClassType::setLocalProxyMaker(LocalProxyMaker maker) const noexcept { localProxyMaker_ = std::move(maker); }

std::shared_ptr<impl::NativeObjectProxy> ClassType::createLocalProxy(NativeObject* object,
                                                                     const std::string& localPrefix) const
{
  return localProxyMaker_ != nullptr ? localProxyMaker_(object, localPrefix)
                                     : spec_.localProxyMaker(object, localPrefix);
}

bool ClassType::hasProxyMakers() const noexcept
{
  return spec_.remoteProxyMaker != nullptr && spec_.localProxyMaker != nullptr;
}

bool ClassType::equals(const Type& other) const noexcept
{
  const auto* otherClass = other.asClassType();
  return other.isClassType() && (otherClass == this || spec_ == otherClass->spec_);
}

}  // namespace sen
