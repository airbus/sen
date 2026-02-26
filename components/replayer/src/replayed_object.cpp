// === replayed_object.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "replayed_object.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/db/event.h"
#include "sen/db/property_change.h"
#include "sen/db/snapshot.h"

// std
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::replayer
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

class FieldGetterVisitor: public TypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(FieldGetterVisitor)

public:
  FieldGetterVisitor(Var value, Span<uint16_t> fields): value_(std::move(value)), fields_(std::move(fields)) {}

  ~FieldGetterVisitor() override = default;

  [[nodiscard]] impl::FieldValueGetter&& takeResult() { return std::move(result_); }

public:
  void apply(const Type& type) override
  {
    std::string err;
    err.append("trying to extract fields from the non-native type ");
    err.append(type.getName());
    throwRuntimeError(err);
  }

  void apply(const EnumType& type) override
  {
    uint32_t key =
      value_.holds<std::string>() ? type.getEnumFromName(value_.get<std::string>())->key : value_.getCopyAs<uint32_t>();
    result_.getterFunc = [key]() { return key; };
  }

  void apply(const BoolType& /*type*/) override
  {
    result_.getterFunc = [val = value_.get<bool>()]() { return val; };
  }

  void apply(const UInt8Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<uint8_t>()]() { return val; };
  }

  void apply(const Int16Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<int16_t>()]() { return val; };
  }

  void apply(const UInt16Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<uint16_t>()]() { return val; };
  }

  void apply(const Int32Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<int32_t>()]() { return val; };
  }

  void apply(const UInt32Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<uint32_t>()]() { return val; };
  }

  void apply(const Int64Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<int64_t>()]() { return val; };
  }

  void apply(const UInt64Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<uint64_t>()]() { return val; };
  }

  void apply(const Float32Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<float32_t>()]() { return val; };
  }

  void apply(const Float64Type& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<float64_t>()]() { return val; };
  }

  void apply(const DurationType& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<Duration>()]() { return val.getNanoseconds(); };
  }

  void apply(const TimestampType& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<TimeStamp>()]() { return val.sinceEpoch().getNanoseconds(); };
  }

  void apply(const StringType& /*type*/) override
  {
    result_.getterFunc = [val = value_.getCopyAs<std::string>()]() { return val; };
  }

  void apply(const StructType& type) override
  {
    const auto& fields = type.getFields();

    if (fields.empty())
    {
      std::string err;
      err.append("cannot extract fields out of struct ");
      err.append(type.getQualifiedName());
      err.append(" because it has no fields");
      throwRuntimeError(err);
    }

    auto index = fields_[0];
    if (index >= fields.size())
    {
      std::string err;
      err.append("invalid field index ");
      err.append(std::to_string(index));
      err.append(" for struct ");
      err.append(type.getQualifiedName());
      throwRuntimeError(err);
    }

    FieldGetterVisitor visitor(std::move(value_), fields_.subspan(1U));
    fields[index].type->accept(visitor);
    result_ = visitor.takeResult();
  }

  void apply(const VariantType& type) override
  {
    const auto& fields = type.getFields();

    if (fields.empty())
    {
      std::string err;
      err.append("cannot extract fields out of variant ");
      err.append(type.getQualifiedName());
      err.append(" because it has no fields");
      throwRuntimeError(err);
    }

    auto index = fields_[0];
    if (index >= fields.size())
    {
      std::string err;
      err.append("invalid field index ");
      err.append(std::to_string(index));
      err.append(" for variant ");
      err.append(type.getQualifiedName());
      throwRuntimeError(err);
    }

    FieldGetterVisitor visitor(std::move(value_), fields_.subspan(1U));
    fields[index].type->accept(visitor);
    result_ = visitor.takeResult();
  }

  void apply(const QuantityType& type) override
  {
    FieldGetterVisitor visitor(std::move(value_), fields_);
    type.getElementType()->accept(visitor);
    result_ = visitor.takeResult();
  }

  void apply(const AliasType& type) override
  {
    FieldGetterVisitor visitor(std::move(value_), fields_);
    type.getAliasedType()->accept(visitor);
    result_ = visitor.takeResult();
  }

  void apply(const OptionalType& type) override
  {
    FieldGetterVisitor visitor(std::move(value_), fields_);
    type.getType()->accept(visitor);
    result_ = visitor.takeResult();
  }

private:
  Var value_;
  impl::FieldValueGetter result_ {};
  Span<uint16_t> fields_;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// ReplayedObject
//--------------------------------------------------------------------------------------------------------------

ReplayedObject::ReplayedObject(const db::Snapshot& snapshot, TimeStamp timeStamp)
  : NativeObject(snapshot.getName())
  , originalId_(snapshot.getObjectId())
  , meta_(snapshot.getType())
  , lastCommitTime_(std::move(timeStamp))
  , allProps_(meta_->getProperties(ClassType::SearchMode::includeParents))
{
  for (const auto& prop: allProps_)
  {
    varCache_.try_emplace(prop->getId(), snapshot.getPropertyAsVariant(prop.get()));
    timeCache_.try_emplace(prop->getId(), timeStamp);

    // create the buffer
    auto [iter, success] = bufferCache_.try_emplace(prop->getId());

    // store the data
    auto span = snapshot.getPropertyAsBuffer(prop.get());
    if (!span.empty())
    {
      iter->second.resize(span.size());
      std::memcpy(iter->second.data(), span.data(), span.size());
    }
  }

  changedProperties_.reserve(allProps_.size());
}

ReplayedObject::~ReplayedObject() = default;

const PropertyList& ReplayedObject::getAllProps() const noexcept { return allProps_; }

TimeStamp ReplayedObject::getLastCommitTime() const noexcept { return lastCommitTime_; }

ConstTypeHandle<ClassType> ReplayedObject::getClass() const noexcept { return meta_; }

TimeStamp ReplayedObject::getPropertyLastTime(const Property* property) const
{
  if (auto itr = timeCache_.find(property->getId()); itr != timeCache_.end())
  {
    return itr->second;
  }
  return lastCommitTime_;
}

void ReplayedObject::senImplRemoveTypedConnection(ConnId id)
{
  std::ignore = id;
  throw std::logic_error("calling senImplRemoveTypedConnection on a ReplayedObject");
}

Var ReplayedObject::senImplGetPropertyImpl(MemberHash propertyId) const
{
  auto itr = varCache_.find(propertyId);
  return itr != varCache_.end() ? itr->second : Var {};
}

uint32_t ReplayedObject::senImplComputeMaxReliableSerializedPropertySizeImpl() const
{
  uint32_t maxSize = 0U;

  for (const auto& prop: allProps_)
  {
    const auto category = prop->getCategory();
    if (category == PropertyCategory::dynamicRO || category == PropertyCategory::dynamicRW)
    {
      auto itr = bufferCache_.find(prop->getId());
      if (itr != bufferCache_.end())
      {
        maxSize = std::max(maxSize, static_cast<uint32_t>(itr->second.size()));
      }
    }
  }

  return maxSize;
}

void ReplayedObject::senImplCommitImpl(TimeStamp time)
{
  // we ignore the 'real' time, but use the one set by the replay
  std::ignore = time;

  if (!changedProperties_.empty())
  {
    for (const auto& [propId, propData]: changedProperties_)
    {
      bufferCache_[propId->getId()] = propData->buffer;
      varCache_[propId->getId()] = propData->var;
      timeCache_[propId->getId()] = propData->time;
    }
  }
}

void ReplayedObject::flushPendingChanges(TimeStamp entryTime)
{
  changedProperties_.clear();
  for (const auto& change: pendingChanges_)
  {
    change();
  }
  lastCommitTime_ = entryTime;
  pendingChanges_.clear();
}

void ReplayedObject::inject(TimeStamp entryTime, const db::PropertyChange& propertyChange)
{
  pendingChanges_.emplace_back(
    [this, entryTime, propertyChange]()
    {
      auto changeData = std::make_unique<ChangedPropertyData>();
      auto span = propertyChange.getValueAsBuffer();

      // save the time, var, and buffer
      changeData->time = entryTime;
      changeData->var = propertyChange.getValueAsVariant();
      changeData->buffer.resize(span.size());
      std::memcpy(changeData->buffer.data(), span.data(), span.size());

      changedProperties_[propertyChange.getProperty()] = std::move(changeData);
      senImplEventEmitted(propertyChange.getProperty()->getId(), {}, EventInfo {entryTime});
    });
}

void ReplayedObject::inject(TimeStamp entryTime, const db::Event& event)
{
  pendingChanges_.emplace_back(
    [this, entryTime, event]()
    {
      auto variantsPtr = std::make_shared<std::vector<Var>>(event.getArgsAsVariants());
      auto bufferPtr = std::make_shared<std::vector<uint8_t>>();
      {
        auto span = event.getArgsAsBuffer();
        bufferPtr->resize(span.size());
        std::memcpy(bufferPtr->data(), span.data(), span.size());
      }

      addWorkToQueue(
        [entryTime, ev = event.getEvent(), buffer = std::move(bufferPtr), variants = std::move(variantsPtr), this]()
        {
          const auto serializedSize = static_cast<uint32_t>(buffer->size());

          getOutputEventQueue()->push({ev->getId(),
                                       entryTime,
                                       [buf = buffer](OutputStream& out)
                                       { std::memcpy(out.getWriter().advance(buf->size()), buf->data(), buf->size()); },
                                       getId(),
                                       ev->getTransportMode(),
                                       serializedSize});

          senImplEventEmitted(ev->getId(), [args = variants]() { return *args; }, EventInfo {entryTime});
        },
        impl::cannotBeDropped(event.getEvent()->getTransportMode()));
    });
}

void ReplayedObject::inject(TimeStamp entryTime, const db::Snapshot& snapshot)
{
  pendingChanges_.emplace_back(
    [this, entryTime, snapshot]()
    {
      for (const auto& prop: allProps_)
      {
        auto span = snapshot.getPropertyAsBuffer(prop.get());

        const auto& currentBuffer = bufferCache_.find(prop->getId())->second;

        // check if the property has changed, and store the change if needed
        if (currentBuffer.size() != span.size() && std::memcmp(currentBuffer.data(), span.data(), span.size()) != 0)
        {
          auto itr = changedProperties_.find(prop.get());
          if (itr == changedProperties_.end())
          {
            auto [pos, done] = changedProperties_.insert({prop.get(), std::make_unique<ChangedPropertyData>()});
            itr = pos;
          }

          // copy the buffer
          itr->second->buffer.resize(span.size());
          std::memcpy(itr->second->buffer.data(), span.data(), span.size());

          // copy the variant
          itr->second->var = snapshot.getPropertyAsVariant(prop.get());
          itr->second->time = entryTime;
        }
      }

      lastCommitTime_ = entryTime;
    });
}

void ReplayedObject::senImplWriteChangedPropertiesToStream(OutputStream& confirmed,
                                                           impl::BufferProvider uni,
                                                           impl::BufferProvider multi)
{
  for (const auto& [prop, change]: changedProperties_)
  {
    switch (prop->getTransportMode())
    {
      case TransportMode::unicast:
        writePropertyToStream(uni, prop, change->buffer);
        break;

      case TransportMode::multicast:
        writePropertyToStream(multi, prop, change->buffer);
        break;

      case TransportMode::confirmed:
        writePropertyToStream(confirmed, prop, change->buffer);
        break;
    }
  }
}

void ReplayedObject::senImplWriteAllPropertiesToStream(OutputStream& out) const
{
  for (const auto& prop: allProps_)
  {
    auto itr = bufferCache_.find(prop->getId());
    if (itr != bufferCache_.end())
    {
      writePropertyToStream(out, prop.get(), itr->second);
    }
  }
}

void ReplayedObject::senImplWriteStaticPropertiesToStream(OutputStream& out) const
{
  for (const auto& prop: allProps_)
  {
    if (const auto category = prop->getCategory();
        category == PropertyCategory::staticRO || category == PropertyCategory::staticRW)
    {
      if (const auto itr = bufferCache_.find(prop->getId()); itr != bufferCache_.end())
      {
        writePropertyToStream(out, prop.get(), itr->second);
      }
    }
  }
}

void ReplayedObject::senImplWriteDynamicPropertiesToStream(OutputStream& out) const
{
  for (const auto& prop: allProps_)
  {
    if (const auto category = prop->getCategory();
        category == PropertyCategory::dynamicRO || category == PropertyCategory::dynamicRW)
    {
      if (const auto itr = bufferCache_.find(prop->getId()); itr != bufferCache_.end())
      {
        writePropertyToStream(out, prop.get(), itr->second);
      }
    }
  }
}

void ReplayedObject::writePropertyToStream(impl::BufferProvider& provider,
                                           const Property* property,
                                           const std::vector<uint8_t>& valueBuffer) const
{
  const auto id = property->getId().get();
  const auto valueSize = static_cast<uint32_t>(valueBuffer.size());

  auto writer = provider(impl::getSerializedSize(id) + impl::getSerializedSize(valueSize) + valueSize);
  OutputStream out(writer);
  out.writeUInt32(id);
  out.writeUInt32(valueSize);
  std::memcpy(out.getWriter().advance(valueSize), valueBuffer.data(), valueBuffer.size());
}

void ReplayedObject::writePropertyToStream(OutputStream& out,
                                           const Property* property,
                                           const std::vector<uint8_t>& valueBuffer) const
{
  out.writeUInt32(property->getId().get());
  out.writeUInt32(static_cast<uint32_t>(valueBuffer.size()));
  std::memcpy(out.getWriter().advance(valueBuffer.size()), valueBuffer.data(), valueBuffer.size());
}

void ReplayedObject::senImplStreamCall(MemberHash methodId, InputStream& in, StreamCallForwarder&& func)
{
  std::ignore = in;

  // only support calls to getter methods
  for (const auto& prop: allProps_)
  {
    if (prop->getGetterMethod().getId() == methodId)
    {
      func([this, prop](OutputStream& out)
           { writePropertyToStream(out, prop.get(), bufferCache_.find(prop->getId())->second); });
      return;
    }
  }

  std::string err;
  err.append("object ");
  err.append(getName());
  err.append(" is being replayed. Methods cannot be called");
  throwRuntimeError(err);
}

void ReplayedObject::senImplVariantCall(MemberHash methodId, const VarList& args, VariantCallForwarder&& func)
{
  std::ignore = args;

  // only support calls to getter methods
  for (const auto& prop: allProps_)
  {
    if (prop->getGetterMethod().getId() == methodId)
    {
      func([this, prop](Var& ret) { ret = getPropertyUntyped(prop.get()); });
      return;
    }
  }

  std::string err;
  err.append("object ");
  err.append(getName());
  err.append(" is being replayed. Methods cannot be called");
  throwRuntimeError(err);
}

impl::FieldValueGetter ReplayedObject::senImplGetFieldValueGetter(MemberHash propertyId, Span<uint16_t> fields) const
{
  const auto* prop = meta_->searchPropertyById(propertyId);
  if (!prop)
  {
    std::string err;
    err.append("property with id ");
    err.append(std::to_string(propertyId.get()));
    err.append(" not found in object ");
    err.append(getName());
    err.append(" of class ");
    err.append(meta_->getQualifiedName());
    throwRuntimeError(err);
  }

  FieldGetterVisitor visitor(varCache_.find(propertyId)->second, fields);
  prop->getType()->accept(visitor);

  return visitor.takeResult();
}

Var ReplayedObject::senImplGetNextPropertyUntyped(::sen::MemberHash propertyId) const
{
  std::ignore = propertyId;

  std::string err;
  err.append("object ");
  err.append(getName());
  err.append(" is being replayed. Getters for next property values cannot be called");
  throwRuntimeError(err);
}

void ReplayedObject::senImplSetNextPropertyUntyped(::sen::MemberHash propertyId, const Var& value)
{
  std::ignore = propertyId;
  std::ignore = value;

  std::string err;
  err.append("object ");
  err.append(getName());
  err.append(" is being replayed. Setters cannot be called");
  throwRuntimeError(err);
}

void ReplayedObject::invokeAllPropertyCallbacks()
{
  // no code needed - replayed objects are internal to the replayer, and this functionality is not needed
}

}  // namespace sen::components::replayer
