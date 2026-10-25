// === generic_local_object.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/detail/generic_local_object.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/encoded_field_reader.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/detail/property_flags.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/detail/generic_local_object_proxy.h"

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
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

/// Writes one property update record: the id, the value's size, the value.
void writeRecord(OutputStream& out, uint32_t id, const std::vector<uint8_t>& value)
{
  out.writeUInt32(id);
  out.writeUInt32(static_cast<uint32_t>(value.size()));
  if (!value.empty())
  {
    std::memcpy(out.getWriter().advance(value.size()), value.data(), value.size());
  }
}

void writeRecord(::sen::impl::BufferProvider& provider, uint32_t id, const std::vector<uint8_t>& value)
{
  auto writer = provider(recordHeaderSize + value.size());
  OutputStream out(writer);
  writeRecord(out, id, value);
}

/// The error a getter call throws for a property with no value.
[[nodiscard]] std::string noValueYet(const Property& property, const std::string& object)
{
  return "the property " + std::string(property.getName()) + " of " + object + " has no value yet";
}

/// Decodes a property's value into a Var; an empty value gives an empty Var.
[[nodiscard]] Var decode(const Property& property, const std::vector<uint8_t>& value)
{
  if (value.empty())
  {
    return {};
  }

  Var result;
  InputStream in(value);
  ::sen::impl::readFromStream(result, in, *property.getType());
  return result;
}

}  // namespace

GenericLocalObject::GenericLocalObject(const std::string& name, ConstTypeHandle<ClassType> type)
  : NativeObject(name)
  , type_(std::move(type))
  , properties_(type_->getProperties(ClassType::SearchMode::includeParents))
  , slots_(properties_.size())
{
  // A generated proxy casts its object to the generated class, which this is not.
  if (type_->hasProxyMakers())
  {
    throwRuntimeError("cannot hold the object " + name + " as a GenericLocalObject: its class " +
                      std::string(type_->getQualifiedName()) +
                      " has proxy makers of its own, as generated code does, and they cannot read it");
  }

  for (std::size_t index = 0U; index < properties_.size(); ++index)
  {
    auto& slot = slots_[index];
    slot.property = properties_[index].get();
    slot.dynamic = isDynamic(*slot.property);
    byId_.emplace(slot.property->getId().get(), index);
  }

  pending_.reserve(slots_.size());
  changed_.reserve(slots_.size());
  records_.reserve(slots_.size());
}

GenericLocalObject::~GenericLocalObject() = default;

ConstTypeHandle<ClassType> GenericLocalObject::getClass() const noexcept { return type_; }

//--------------------------------------------------------------------------------------------------------------
// Values and changes
//--------------------------------------------------------------------------------------------------------------

const std::vector<uint8_t>& GenericLocalObject::currentOf(const Slot& slot) noexcept
{
  return slot.values[slot.flags.currentIndex()];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

std::vector<uint8_t>& GenericLocalObject::currentOf(Slot& slot) noexcept
{
  return slot.values[slot.flags.currentIndex()];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

const std::vector<uint8_t>& GenericLocalObject::nextOf(const Slot& slot) noexcept
{
  return slot.values[slot.flags.nextIndex()];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

std::vector<uint8_t>& GenericLocalObject::nextOf(Slot& slot) noexcept
{
  return slot.values[slot.flags.nextIndex()];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

void GenericLocalObject::set(std::size_t index, Span<const uint8_t> value)
{
  SEN_EXPECT(index < slots_.size());

  // empty marks a property that was never set
  if (value.empty())
  {
    throwRuntimeError("cannot set the property " + std::string(slots_[index].property->getName()) + " of " + getName() +
                      " to an empty value: its type has no bytes in Sen's encoding, which a GenericLocalObject cannot "
                      "hold, or the value was cut short");
  }

  auto writeLock = createWriterLock();
  setLocked(index, value);
}

void GenericLocalObject::setLocked(std::size_t index, Span<const uint8_t> value)
{
  auto& slot = slots_[index];
  if (!slot.dynamic)
  {
    currentOf(slot).assign(value.begin(), value.end());
    return;
  }

  if (!slot.pending)
  {
    if (Span<const uint8_t>(currentOf(slot)) == value)
    {
      return;  // unchanged, as in a generated setter
    }

    slot.pending = true;
    std::ignore = slot.flags.advanceNext();
    pending_.push_back(index);
  }

  nextOf(slot).assign(value.begin(), value.end());
}

bool GenericLocalObject::apply(Span<const uint8_t> records)
{
  auto writeLock = createWriterLock();

  // Check every record before setting any, so that a bad one leaves the object unchanged.
  records_.clear();
  InputStream in(records);
  while (!in.atEnd())
  {
    if (records.size() - in.getPosition() < recordHeaderSize)
    {
      return false;
    }

    uint32_t id = 0U;
    uint32_t size = 0U;
    in.readUInt32(id);
    in.readUInt32(size);

    const auto offset = in.getPosition();
    const auto itr = byId_.find(id);
    if (itr == byId_.end() || size == 0U || size > records.size() - offset)
    {
      return false;
    }

    records_.push_back({itr->second, offset, size});
    in.setPosition(offset + size);
  }

  for (const auto& record: records_)
  {
    setLocked(record.index, records.subspan(record.offset, record.size));
  }
  return true;
}

Span<const uint8_t> GenericLocalObject::current(std::size_t index) const noexcept
{
  SEN_EXPECT(index < slots_.size());
  return currentOf(slots_[index]);
}

const ::sen::impl::PropertyFlags& GenericLocalObject::flags(std::size_t index) const noexcept
{
  SEN_EXPECT(index < slots_.size());
  return slots_[index].flags;
}

Span<const uint8_t> GenericLocalObject::getPropertyEncoding(MemberHash propertyId) const
{
  const auto* slot = slotOf(propertyId);
  return slot == nullptr ? Span<const uint8_t> {} : Span<const uint8_t>(currentOf(*slot));
}

const GenericLocalObject::Slot* GenericLocalObject::slotOf(MemberHash propertyId) const noexcept
{
  const auto itr = byId_.find(propertyId.get());
  return itr == byId_.end() ? nullptr : &slots_[itr->second];
}

void GenericLocalObject::senImplCommitImpl(TimeStamp time)
{
  // clear the changed marks the previous commit set
  for (const auto index: changed_)
  {
    if (!slots_[index].pending)
    {
      slots_[index].flags.advanceCurrent();
    }
  }
  changed_.clear();

  for (const auto index: pending_)
  {
    auto& slot = slots_[index];
    slot.flags.advanceCurrent();  // the next value becomes current, and is marked changed
    slot.pending = false;
    changed_.push_back(index);

    // notify the property's local subscribers
    senImplEventEmitted(slot.property->getId(), []() { return VarList {}; }, EventInfo {time});
  }
  pending_.clear();
}

std::shared_ptr<::sen::impl::NativeObjectProxy> GenericLocalObject::senImplMakeLocalProxy(
  const std::string& localPrefix)
{
  return makeGenericLocalProxy(this, this, nullptr, localPrefix);
}

void GenericLocalObject::invokeAllPropertyCallbacks()
{
  const EventInfo info {getLastCommitTime()};
  for (const auto& slot: slots_)
  {
    if (slot.dynamic)
    {
      senImplEventEmitted(slot.property->getId(), []() { return VarList {}; }, info);
    }
  }
}

void GenericLocalObject::senImplRemoveTypedConnection(ConnId id)
{
  std::ignore = id;  // only generated code makes typed connections
}

//--------------------------------------------------------------------------------------------------------------
// Streams
//--------------------------------------------------------------------------------------------------------------

void GenericLocalObject::senImplWriteChangedPropertiesToStream(OutputStream& confirmed,
                                                               ::sen::impl::BufferProvider uni,
                                                               ::sen::impl::BufferProvider multi)
{
  for (const auto index: changed_)
  {
    // a commit made it current, so it is not empty
    const auto& slot = slots_[index];
    const auto& value = currentOf(slot);
    const auto id = slot.property->getId().get();
    switch (slot.property->getTransportMode())
    {
      case TransportMode::unicast:
        writeRecord(uni, id, value);
        break;
      case TransportMode::multicast:
        writeRecord(multi, id, value);
        break;
      case TransportMode::confirmed:
        writeRecord(confirmed, id, value);
        break;
    }
  }
}

void GenericLocalObject::writeAll(OutputStream& out, bool statics, bool dynamics) const
{
  for (const auto& slot: slots_)
  {
    const auto& value = currentOf(slot);
    if (!value.empty() && (slot.dynamic ? dynamics : statics))
    {
      writeRecord(out, slot.property->getId().get(), value);
    }
  }
}

void GenericLocalObject::senImplWriteAllPropertiesToStream(OutputStream& out) const { writeAll(out, true, true); }

void GenericLocalObject::senImplWriteStaticPropertiesToStream(OutputStream& out) const { writeAll(out, true, false); }

void GenericLocalObject::senImplWriteDynamicPropertiesToStream(OutputStream& out) const { writeAll(out, false, true); }

uint32_t GenericLocalObject::senImplComputeMaxReliableSerializedPropertySizeImpl() const
{
  std::size_t maxSize = 0U;
  for (const auto& slot: slots_)
  {
    if (slot.dynamic && slot.property->getTransportMode() == TransportMode::confirmed)
    {
      maxSize = std::max(maxSize, currentOf(slot).size());
    }
  }
  return static_cast<uint32_t>(maxSize);
}

//--------------------------------------------------------------------------------------------------------------
// Untyped access
//--------------------------------------------------------------------------------------------------------------

Var GenericLocalObject::senImplGetPropertyImpl(MemberHash propertyId) const
{
  const auto* slot = slotOf(propertyId);
  return slot == nullptr ? Var {} : decode(*slot->property, currentOf(*slot));
}

Var GenericLocalObject::senImplGetNextPropertyUntyped(MemberHash propertyId) const
{
  const auto* slot = slotOf(propertyId);
  if (slot == nullptr)
  {
    return {};
  }
  return decode(*slot->property, slot->pending ? nextOf(*slot) : currentOf(*slot));
}

void GenericLocalObject::senImplSetNextPropertyUntyped(MemberHash propertyId, const Var& value)
{
  const auto itr = byId_.find(propertyId.get());
  if (itr == byId_.end())
  {
    throwRuntimeError("no property with id " + std::to_string(propertyId.get()) + " in " + getName());
  }

  std::vector<uint8_t> bytes;
  ResizableBufferWriter writer(bytes);
  OutputStream out(writer);
  ::sen::impl::writeToStream(value, out, *slots_[itr->second].property->getType());
  set(itr->second, bytes);
}

//--------------------------------------------------------------------------------------------------------------
// Calls
//--------------------------------------------------------------------------------------------------------------

const Property* GenericLocalObject::getterOf(MemberHash methodId) const noexcept
{
  for (const auto& property: properties_)
  {
    if (property->getGetterMethod().getId() == methodId)
    {
      return property.get();
    }
  }
  return nullptr;
}

std::string GenericLocalObject::refusal(MemberHash methodId) const
{
  const auto* method = type_->searchMethodById(methodId);
  const auto what = method == nullptr ? "the method with id " + std::to_string(methodId.get())
                                      : "'" + std::string(method->getName()) + "'";

  return "cannot call " + what + " on " + getName() + ", an object of " + std::string(type_->getQualifiedName()) +
         " held as its encoding: it answers only its properties' getters";
}

void GenericLocalObject::senImplStreamCall(MemberHash methodId, InputStream& in, StreamCallForwarder&& fwd)
{
  std::ignore = in;

  if (const auto* property = getterOf(methodId); property != nullptr)
  {
    fwd(
      [slot = slotOf(property->getId()), message = noValueYet(*property, getName())](OutputStream& out)
      {
        const auto& value = currentOf(*slot);
        if (value.empty())
        {
          throwRuntimeError(message);
        }
        std::memcpy(out.getWriter().advance(value.size()), value.data(), value.size());
      });
    return;
  }

  fwd([message = refusal(methodId)](OutputStream& /*out*/) { throwRuntimeError(message); });
}

void GenericLocalObject::senImplVariantCall(MemberHash methodId, const VarList& args, VariantCallForwarder&& fwd)
{
  std::ignore = args;

  if (const auto* property = getterOf(methodId); property != nullptr)
  {
    // a property with no value throws, as in a stream call
    fwd(
      [slot = slotOf(property->getId()), message = noValueYet(*property, getName())](Var& ret)
      {
        const auto& value = currentOf(*slot);
        if (value.empty())
        {
          throwRuntimeError(message);
        }
        ret = decode(*slot->property, value);
      });
    return;
  }

  fwd([message = refusal(methodId)](Var& /*ret*/) { throwRuntimeError(message); });
}

//--------------------------------------------------------------------------------------------------------------
// Queries
//--------------------------------------------------------------------------------------------------------------

::sen::impl::FieldValueGetter GenericLocalObject::senImplGetFieldValueGetter(MemberHash propertyId,
                                                                             Span<uint16_t> fields) const
{
  const auto* slot = slotOf(propertyId);
  if (slot == nullptr)
  {
    throwRuntimeError("no property with id " + std::to_string(propertyId.get()) + " in " +
                      std::string(type_->getQualifiedName()));
  }

  // Span<T> does not convert to Span<const T>
  const Span<const uint16_t> path(fields.data(), fields.size());
  return {[this, slot, reader = ::sen::impl::EncodedFieldReader::resolve(*slot->property->getType(), path)]()
          {
            // a commit may switch the current value from another thread; the filter's lock is
            // already held, so this one is always taken second
            auto readLock = createReaderLock();
            return reader.read(currentOf(*slot));
          },
          slot->dynamic ? &slot->flags : nullptr};
}

}  // namespace sen::kernel::impl
