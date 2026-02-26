// === generic_remote_object.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "generic_remote_object.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/object.h"

// std
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>

namespace sen::kernel::impl
{

GenericRemoteObject::GenericRemoteObject(::sen::impl::RemoteObjectInfo&& info)
  : ::sen::impl::RemoteObject(std::move(info))
{
  metaProperties_ = getClass()->getProperties(ClassType::SearchMode::includeParents);
}

GenericRemoteObject::~GenericRemoteObject() = default;

void GenericRemoteObject::copyStateFrom(const RemoteObject& other)
{
  const auto otherRemote = dynamic_cast<const GenericRemoteObject*>(&other);
  SEN_ASSERT(otherRemote != nullptr && "Remote object is not of required type 'GenericRemoteObject'");
  properties_ = otherRemote->properties_;
  RemoteObject::copyStateFrom(other);
}

void GenericRemoteObject::drainInputs()
{
  for (auto& [id, data]: properties_)
  {
    data.flags.advanceCurrent();
  }

  EventInfo info {};
  info.creationTime = getLastCommitTime();

  for (const auto& [id, data]: properties_)
  {
    if (data.flags.changedInLastCycle())
    {
      senImplEventEmitted(id, []() { return VarList {}; }, info);
    }
  }
}

void GenericRemoteObject::eventReceived(MemberHash eventId, TimeStamp creationTime, const Span<const uint8_t>& args)
{
  const auto* event = getClass()->searchEventById(eventId);
  if (event == nullptr)
  {
    return;
  }

  const auto& eventArgs = event->getArgs();
  VarList values;
  values.reserve(eventArgs.size());

  InputStream in(args);
  for (const auto& arg: eventArgs)
  {
    values.emplace_back();
    ::sen::impl::readFromStream(values.back(), in, *arg.type);
  }

  EventInfo info {};
  info.creationTime = creationTime;

  auto getter = [vars = std::move(values)]() { return vars; };
  senImplEventEmitted(eventId, std::move(getter), info);
}

bool GenericRemoteObject::readPropertyFromStream(MemberHash id, InputStream& in, bool advanceCurrent)
{
  const auto* prop = getClass()->searchPropertyById(id);
  if (!prop)
  {
    return false;
  }

  auto propItr = properties_.find(id);
  if (propItr == properties_.end())
  {
    auto [newItr, done] = properties_.try_emplace(id);
    propItr = newItr;
  }
  auto& propData = propItr->second;

  ::sen::impl::readFromStream(propData.value.at(propData.flags.advanceNext()), in, *prop->getType());
  if (advanceCurrent)
  {
    propData.flags.advanceCurrent();
  }
  return true;
}

Var GenericRemoteObject::senImplGetPropertyImpl(MemberHash propertyId) const
{
  std::lock_guard lock(*this);
  auto itr = properties_.find(propertyId);
  return itr == properties_.end() ? Var() : itr->second.value.at(itr->second.flags.currentIndex());
}

void GenericRemoteObject::serializeMethodArgumentsFromVariants(MemberHash methodId,
                                                               const VarList& args,
                                                               OutputStream& out) const
{
  const auto* method = searchMethodById(methodId);

  if (method == nullptr)
  {
    std::string err;
    err.append("invalid method id while trying to serialize arguments on class ");
    err.append(getClass()->getQualifiedName());
    err.append(" on a generic remote object");
    throwRuntimeError(err);
  }

  const auto& methodArgs = method->getArgs();

  if (methodArgs.size() != args.size())
  {
    std::string err;
    err.append("incorrect number of args (");
    err.append(std::to_string(args.size()));
    err.append(") when trying to serialize arguments of method ");
    err.append(method->getName());
    err.append(" in class ");
    err.append(getClass()->getQualifiedName());
    err.append(" on a generic remote object. Expecting ");
    err.append(std::to_string(methodArgs.size()));
    err.append(".");
    throwRuntimeError(err);
  }

  for (std::size_t i = 0; i < args.size(); ++i)
  {
    ::sen::impl::writeToStream(args[i], out, *methodArgs[i].type);
  }
}

Var GenericRemoteObject::deserializeMethodReturnValueAsVariant(MemberHash methodId, InputStream& in) const
{
  const auto* method = searchMethodById(methodId);

  if (method == nullptr)
  {
    std::string err;
    err.append("invalid method id while trying to deserialize return value on class ");
    err.append(getClass()->getQualifiedName());
    err.append(" on a generic remote object");
    throwRuntimeError(err);
  }

  auto type = method->getReturnType();

  Var result;
  ::sen::impl::readFromStream(result, in, *type);
  return result;
}

void GenericRemoteObject::senImplRemoveTypedConnection(ConnId id)
{
  std::ignore = id;  // nothing to do, as no typed connections are possible
}

void GenericRemoteObject::senImplWriteAllPropertiesToStream(OutputStream& out) const
{
  for (const auto& prop: metaProperties_)
  {
    const auto id = prop->getId();
    const auto& itr = properties_.find(id);
    if (itr != properties_.end())
    {
      const auto& var = itr->second.value[itr->second.flags.currentIndex()];  // NOLINT
      const auto* type = prop->getType().type();

      out.writeUInt32(id.get());
      out.writeUInt32(::sen::impl::getSerializedSize(var, type));
      ::sen::impl::writeToStream(var, out, *type);
    }
  }
}

void GenericRemoteObject::senImplWriteStaticPropertiesToStream(OutputStream& out) const
{
  for (const auto& prop: metaProperties_)
  {
    if (const auto category = prop->getCategory();
        category == PropertyCategory::staticRO || category == PropertyCategory::staticRW)
    {
      const auto id = prop->getId();
      if (const auto itr = properties_.find(id); itr != properties_.end())
      {
        const auto& var = itr->second.value[itr->second.flags.currentIndex()];  // NOLINT
        const auto* type = prop->getType().type();

        out.writeUInt32(id.get());
        out.writeUInt32(::sen::impl::getSerializedSize(var, type));
        ::sen::impl::writeToStream(var, out, *type);
      }
    }
  }
}

void GenericRemoteObject::senImplWriteDynamicPropertiesToStream(OutputStream& out) const
{
  for (const auto& prop: metaProperties_)
  {
    if (const auto category = prop->getCategory();
        category == PropertyCategory::dynamicRO || category == PropertyCategory::dynamicRW)
    {
      const auto id = prop->getId();
      if (const auto itr = properties_.find(id); itr != properties_.end())
      {
        const auto& var = itr->second.value[itr->second.flags.currentIndex()];  // NOLINT
        const auto* type = prop->getType().type();

        out.writeUInt32(id.get());
        out.writeUInt32(::sen::impl::getSerializedSize(var, type));
        ::sen::impl::writeToStream(var, out, *type);
      }
    }
  }
}

void GenericRemoteObject::invokeAllPropertyCallbacks()
{
  EventInfo info {};
  info.creationTime = getLastCommitTime();

  for (const auto& [id, data]: properties_)
  {
    senImplEventEmitted(id, []() { return VarList {}; }, info);
  }
}

const Method* GenericRemoteObject::searchMethodById(MemberHash methodId) const
{
  if (const auto* method = getClass()->searchMethodById(methodId); method)
  {
    return method;
  }

  // look for the method in the properties getters/setters
  for (const auto& property: getClass()->getProperties(ClassType::SearchMode::includeParents))
  {
    if (auto& getterMethod = property->getGetterMethod(); getterMethod.getId() == methodId)
    {
      return &getterMethod;
    }

    if (auto& setterMethod = property->getSetterMethod(); setterMethod.getId() == methodId)
    {
      return &setterMethod;
    }
  }

  return nullptr;
}

}  // namespace sen::kernel::impl
