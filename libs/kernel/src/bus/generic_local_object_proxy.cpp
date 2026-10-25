// === generic_local_object_proxy.cpp ==================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/detail/generic_local_object_proxy.h"

// implementation
#include "kernel_impl.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/native_object_proxy.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"

// spdlog
#include <spdlog/logger.h>  // NOLINT (misc-include-cleaner): indirectly used by log function

// std
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace sen::kernel::impl
{

namespace
{

/// A record's header: the property's id and the value's size.
constexpr std::size_t recordHeaderSize = 8U;

/// Whether a property is dynamic.
[[nodiscard]] bool isDynamic(const Property& property) noexcept
{
  const auto category = property.getCategory();
  return category == PropertyCategory::dynamicRO || category == PropertyCategory::dynamicRW;
}

/// Logs, once per class, that no local proxy could be made for its objects.
void reportNoLocalProxy(const ClassType& type, std::string_view reason)
{
  static std::mutex reportedMutex;
  static std::unordered_set<uint32_t> reported;
  {
    std::scoped_lock lock(reportedMutex);
    if (!reported.insert(type.getHash().get()).second)
    {
      return;
    }
  }

  KernelImpl::getKernelLogger()->error(
    "cannot make a local proxy for objects of class '{}', so local subscribers will not see them: {}",
    type.getQualifiedName(),
    reason);
}

}  // namespace

EncodedPropertyValues::~EncodedPropertyValues() = default;

PropertyChangeTimes::~PropertyChangeTimes() = default;

GenericLocalObjectProxy::GenericLocalObjectProxy(NativeObject* owner,
                                                 const EncodedPropertyValues* encodings,
                                                 const PropertyChangeTimes* times,
                                                 std::string_view localPrefix)
  : ::sen::impl::NativeObjectProxy(owner, localPrefix), owner_(owner), encodings_(encodings), times_(times)
{
  const auto properties = owner_->getClass()->getProperties(ClassType::SearchMode::includeParents);
  slots_.resize(properties.size());
  for (std::size_t index = 0U; index < properties.size(); ++index)
  {
    slots_[index].property = properties[index].get();
    slots_[index].dynamic = isDynamic(*properties[index]);
    byId_.emplace(properties[index]->getId().get(), index);
  }

  // take the object's current state, against which later drains detect changes
  drainInputs();
}

GenericLocalObjectProxy::~GenericLocalObjectProxy() = default;

void GenericLocalObjectProxy::drainInputsImpl(TimeStamp lastCommitTime)
{
  // NativeObjectProxy::drainInputs holds the owner's reader lock while this runs

  // After the first drain, an object with encodings is read property by property.
  if (taken_ && encodings_ != nullptr)
  {
    takeEncodings(lastCommitTime);
    return;
  }

  takeWritten(lastCommitTime);
}

void GenericLocalObjectProxy::takeWritten(TimeStamp lastCommitTime)
{
  // Used for the first drain, and for every drain of an object without encodings.
  written_.clear();
  {
    ResizableBufferWriter writer(written_);
    OutputStream out(writer);
    ::sen::impl::writeAllPropertiesToStream(owner_, out);
  }

  // The first drain notifies nothing: nobody can have subscribed to the proxy yet.
  const bool telling = taken_;
  taken_ = true;

  // Each record is the property's id, the value's size and the value.
  InputStream in(written_);
  while (written_.size() - in.getPosition() >= recordHeaderSize)
  {
    uint32_t id = 0U;
    uint32_t size = 0U;
    in.readUInt32(id);
    in.readUInt32(size);
    if (size > written_.size() - in.getPosition())
    {
      return;  // truncated; the kernel's own writers never do this
    }

    const Span<const uint8_t> now(in.advance(size), size);
    const auto itr = byId_.find(id);
    if (itr == byId_.end())
    {
      continue;  // not a property of the class
    }

    auto& slot = slots_[itr->second];
    const bool newValue = !slot.hasValue || Span<const uint8_t>(slot.value) != now;
    if (newValue)
    {
      slot.value.assign(now.begin(), now.end());
      slot.hasValue = true;
    }

    // with change times, notify any property whose time moved, even if its value did not;
    // otherwise a dynamic property whose value changed, with the commit time
    bool changed = newValue && slot.dynamic;
    auto time = lastCommitTime;
    if (times_ != nullptr)
    {
      time = times_->getPropertyChangeTime(slot.property->getId());
      changed = time != slot.time;
      slot.time = time;
    }

    if (telling && changed)
    {
      senImplEventEmitted(slot.property->getId(), []() { return VarList {}; }, EventInfo {time});
    }
  }
}

void GenericLocalObjectProxy::takeEncodings(TimeStamp lastCommitTime)
{
  // Copy only the encodings that differ from the copy, and notify as takeWritten does.
  for (auto& slot: slots_)
  {
    const auto id = slot.property->getId();
    const bool newValue = take(slot, encodings_->getPropertyEncoding(id));

    bool changed = newValue && slot.dynamic;
    auto time = lastCommitTime;
    if (times_ != nullptr)
    {
      time = times_->getPropertyChangeTime(id);
      changed = time != slot.time;
      slot.time = time;
    }

    if (changed)
    {
      senImplEventEmitted(id, []() { return VarList {}; }, EventInfo {time});
    }
  }
}

bool GenericLocalObjectProxy::take(Slot& slot, Span<const uint8_t> now)
{
  // A property that still has no value stays unset, so it is not written.
  if (slot.hasValue ? Span<const uint8_t>(slot.value) == now : now.empty())
  {
    return false;
  }

  slot.value.assign(now.begin(), now.end());
  slot.hasValue = true;
  return true;
}

Var GenericLocalObjectProxy::senImplGetPropertyImpl(MemberHash propertyId) const
{
  // an empty value means the object has no value for the property
  const auto itr = byId_.find(propertyId.get());
  if (itr == byId_.end() || slots_[itr->second].value.empty())
  {
    return {};
  }

  const auto& slot = slots_[itr->second];
  Var result;
  InputStream in(slot.value);
  ::sen::impl::readFromStream(result, in, *slot.property->getType());
  return result;
}

void GenericLocalObjectProxy::writeAll(OutputStream& out, bool statics, bool dynamics) const
{
  for (const auto& slot: slots_)
  {
    if (!slot.hasValue || !(slot.dynamic ? dynamics : statics))
    {
      continue;  // not written by the object, so not written here either
    }

    out.writeUInt32(slot.property->getId().get());
    out.writeUInt32(static_cast<uint32_t>(slot.value.size()));
    if (!slot.value.empty())
    {
      std::memcpy(out.getWriter().advance(slot.value.size()), slot.value.data(), slot.value.size());
    }
  }
}

void GenericLocalObjectProxy::senImplWriteAllPropertiesToStream(OutputStream& out) const { writeAll(out, true, true); }

void GenericLocalObjectProxy::senImplWriteStaticPropertiesToStream(OutputStream& out) const
{
  writeAll(out, true, false);
}

void GenericLocalObjectProxy::senImplWriteDynamicPropertiesToStream(OutputStream& out) const
{
  writeAll(out, false, true);
}

void GenericLocalObjectProxy::senImplRemoveTypedConnection(ConnId id)
{
  // only generated code makes typed connections, so there is none to remove; the owner is not
  // called, since some owners throw
  std::ignore = id;
}

std::shared_ptr<GenericLocalObjectProxy> makeGenericLocalProxy(NativeObject* object,
                                                               const EncodedPropertyValues* encodings,
                                                               const PropertyChangeTimes* times,
                                                               const std::string& localPrefix)
{
  try
  {
    return std::make_shared<GenericLocalObjectProxy>(object, encodings, times, localPrefix);
  }
  catch (const std::exception& err)
  {
    // a throw here would abort the proxy manager's drain and lose the rest of its batch
    reportNoLocalProxy(*object->getClass(), err.what());
    return nullptr;
  }
}

}  // namespace sen::kernel::impl
