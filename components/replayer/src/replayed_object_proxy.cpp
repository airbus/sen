// === replayed_object_proxy.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "replayed_object_proxy.h"

// component
#include "replayed_object.h"

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/native_object_proxy.h"
#include "sen/core/obj/object.h"

// std
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace sen::components::replayer
{

//--------------------------------------------------------------------------------------------------------------
// ReplayedObjectProxy
//--------------------------------------------------------------------------------------------------------------

ReplayedObjectProxy::ReplayedObjectProxy(ReplayedObject* owner, std::string_view localPrefix)
  : ::sen::impl::NativeObjectProxy(owner, localPrefix), guard_(owner->shared_from_this()), owner_(owner)
{
  for (const auto& prop: owner_->getAllProps())
  {
    varCache_.try_emplace(prop->getId(), owner->getPropertyUntyped(prop.get()));
    timeCache_.try_emplace(prop->getId(), owner->getPropertyLastTime(prop.get()));
  }
}

ReplayedObjectProxy::~ReplayedObjectProxy() = default;

void ReplayedObjectProxy::drainInputsImpl(TimeStamp lastCommitTime)
{
  std::ignore = lastCommitTime;

  for (const auto& prop: owner_->getAllProps())
  {
    const auto ownersPropTime = owner_->getPropertyLastTime(prop.get());
    auto timeItr = timeCache_.find(prop->getId());
    if (timeItr->second != ownersPropTime)
    {
      EventInfo info {};
      info.creationTime = ownersPropTime;

      varCache_[prop->getId()] = owner_->getPropertyUntyped(prop.get());
      timeItr->second = ownersPropTime;

      senImplEventEmitted(prop->getId(), []() { return VarList {}; }, info);
    }
  }
}

Var ReplayedObjectProxy::senImplGetPropertyImpl(MemberHash propertyId) const
{
  if (auto itr = varCache_.find(propertyId); itr != varCache_.end())
  {
    return itr->second;
  }
  return {};
}

void ReplayedObjectProxy::senImplWriteAllPropertiesToStream(OutputStream& out) const
{
  std::ignore = out;
  throw std::logic_error("calling senImplWriteAllPropertiesToStream on a ReplayedObjectProxy");
}

void ReplayedObjectProxy::senImplWriteStaticPropertiesToStream(OutputStream& out) const
{
  std::ignore = out;
  throw std::logic_error("calling senImplWriteStaticPropertiesToStream on a ReplayedObjectProxy");
}

void ReplayedObjectProxy::senImplWriteDynamicPropertiesToStream(OutputStream& out) const
{
  std::ignore = out;
  throw std::logic_error("calling senImplWriteStaticPropertiesToStream on a ReplayedObjectProxy");
}

void ReplayedObjectProxy::senImplRemoveTypedConnection(ConnId id)
{
  std::ignore = id;
  throw std::logic_error("calling senImplRemoveTypedConnection on a ReplayedObjectProxy");
}

}  // namespace sen::components::replayer
