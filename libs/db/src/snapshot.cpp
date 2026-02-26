// === snapshot.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/snapshot.h"

// implementation
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"

// std
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sen::db
{

Snapshot::Snapshot(ObjectId objectId,
                   std::string objectName,
                   std::string sessionName,
                   std::string busName,
                   ConstTypeHandle<ClassType> type,
                   std::vector<uint8_t> buffer)
  : objectId_(std::move(objectId))
  , objectName_(std::move(objectName))
  , sessionName_(std::move(sessionName))
  , busName_(std::move(busName))
  , type_(std::move(type))
  , buffer_(std::move(buffer))
{
}

ObjectId Snapshot::getObjectId() const noexcept { return objectId_; }

const std::string& Snapshot::getName() const noexcept { return objectName_; }

const std::string& Snapshot::getSessionName() const noexcept { return sessionName_; }

const std::string& Snapshot::getBusName() const noexcept { return busName_; }

const Var& Snapshot::getPropertyAsVariant(const Property* property) const
{
  // look in the cache
  if (auto varItr = varCache_.find(property); varItr != varCache_.end())
  {
    return varItr->second;
  }

  // not cached, need to get it out of the buffers
  extractBuffersIfNeeded();
  const auto buffer = getPropertyAsBuffer(property);
  if (buffer.empty())
  {
    std::string err;
    err.append("property '");
    err.append(property->getName());
    err.append("' not found in object '");
    err.append(objectName_);
    err.append("' (of class '");
    err.append(property->getType()->getName());
    err.append("')'");
    throwRuntimeError(err);
  }

  // get the variant out of the buffer
  Var varValue;
  InputStream in(buffer);
  ::sen::impl::readFromStream(varValue, in, *property->getType());

  auto [newItr, done] = varCache_.try_emplace(property, std::move(varValue));
  return newItr->second;
}

Span<const uint8_t> Snapshot::getPropertyAsBuffer(const Property* property) const
{
  extractBuffersIfNeeded();

  auto itr = bufferCache_.find(property->getId());

  // if the property is not found, try with the platform-dependent computation
  if (itr == bufferCache_.end())
  {
    itr = bufferCache_.find(computePlatformDependentPropertyId(type_.type(), property));
  }

  return itr != bufferCache_.end() ? itr->second : Span<const uint8_t>();
}

Span<const uint8_t> Snapshot::getAllPropertiesBuffer() const noexcept { return buffer_; }

void Snapshot::extractBuffersIfNeeded() const
{
  if (buffersExtracted_)
  {
    return;
  }

  InputStream in(buffer_);

  while (!in.atEnd())
  {
    uint32_t hash;
    in.readUInt32(hash);

    uint32_t size;
    in.readUInt32(size);

    auto* bufferStart = in.advance(size);

    bufferCache_.try_emplace(hash, Span<const uint8_t>(bufferStart, size));
  }

  buffersExtracted_ = true;
}

}  // namespace sen::db
