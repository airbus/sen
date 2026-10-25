// === generic_local_object_helpers.h ==================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_UNIT_BUS_GENERIC_LOCAL_OBJECT_HELPERS_H
#define SEN_LIBS_KERNEL_TEST_UNIT_BUS_GENERIC_LOCAL_OBJECT_HELPERS_H

// sen
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/detail/generic_local_object.h"

// std
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace generic_test
{

/// A class built at runtime, as from a FOM: a static string and dynamic properties of each
/// transport mode, among them an enum, a struct and an optional.
inline sen::ConstTypeHandle<sen::ClassType> makeTrackClass(const std::string& qualifiedName)
{
  const auto package = qualifiedName.substr(0U, qualifiedName.find_last_of('.'));
  const auto name = qualifiedName.substr(qualifiedName.find_last_of('.') + 1U);

  const auto force = sen::EnumType::make({"Force",
                                          package + ".Force",
                                          "",
                                          {{"neutral", 0U, ""}, {"friendly", 1U, ""}, {"hostile", 2U, ""}},
                                          sen::UInt8Type::get()});

  const std::vector<sen::StructField> positionFields {{"lat", "", sen::Float64Type::get()},
                                                      {"lon", "", sen::Float64Type::get()}};
  const auto position = sen::StructType::make({"Position", package + ".Position", "", positionFields});

  const auto fuel = sen::OptionalType::make({"Fuel", package + ".Fuel", "", sen::Float64Type::get()});

  using sen::PropertyCategory;
  using sen::TransportMode;
  const sen::ClassSpec spec {
    name,
    qualifiedName,
    "",
    {{"callsign", "", sen::StringType::get(), PropertyCategory::staticRW, TransportMode::confirmed},
     {"altitude", "", sen::Float64Type::get(), PropertyCategory::dynamicRO, TransportMode::multicast},
     {"force", "", force, PropertyCategory::dynamicRO, TransportMode::confirmed},
     {"status", "", sen::StringType::get(), PropertyCategory::dynamicRW, TransportMode::unicast},
     {"position", "", position, PropertyCategory::dynamicRO, TransportMode::multicast},
     {"fuel", "", fuel, PropertyCategory::dynamicRO, TransportMode::confirmed}},
    {},
    {},
    sen::MethodSpec {{"constructorHeld", "", {}}, sen::VoidType::get()},
    {},
    false,
    false,
    nullptr,
    nullptr};
  return sen::ClassType::make(spec);
}

/// A parentless generated class copied without its proxy makers, as a gateway would build it.
inline sen::ConstTypeHandle<sen::ClassType> runtimeCopyOf(const sen::ClassType& generated)
{
  std::vector<sen::PropertySpec> properties;
  for (const auto& property: generated.getProperties(sen::ClassType::SearchMode::doNotIncludeParents))
  {
    properties.emplace_back(std::string(property->getName()),
                            std::string(property->getDescription()),
                            property->getType(),
                            property->getCategory(),
                            property->getTransportMode(),
                            property->getCheckedSet(),
                            property->getTags());
  }

  const sen::ClassSpec spec {std::string(generated.getName()),
                             std::string(generated.getQualifiedName()),
                             std::string(generated.getDescription()),
                             std::move(properties),
                             {},
                             {},
                             sen::MethodSpec {{"constructorHeld", "", {}}, sen::VoidType::get()},
                             {},
                             false,
                             false,
                             nullptr,
                             nullptr};
  return sen::ClassType::make(spec);
}

/// The index a GenericLocalObject gives the property named name.
inline std::size_t indexOf(const sen::kernel::impl::GenericLocalObject& object, std::string_view name)
{
  const auto& properties = object.properties();
  for (std::size_t index = 0U; index < properties.size(); ++index)
  {
    if (properties[index]->getName() == name)
    {
      return index;
    }
  }
  throw std::invalid_argument("no property named " + std::string(name));
}

/// The Sen encoding of value, of type.
inline std::vector<uint8_t> encode(const sen::Var& value, const sen::Type& type)
{
  std::vector<uint8_t> bytes;
  sen::ResizableBufferWriter writer(bytes);
  sen::OutputStream out(writer);
  sen::impl::writeToStream(value, out, type);
  return bytes;
}

/// The encoding of value for the property named name of object.
inline std::vector<uint8_t> encodeFor(const sen::kernel::impl::GenericLocalObject& object,
                                      std::string_view name,
                                      const sen::Var& value)
{
  return encode(value, *object.properties()[indexOf(object, name)]->getType());
}

/// Appends a property record (u32 id, u32 size, value), the framing GenericLocalObject::apply reads.
inline void appendRecord(std::vector<uint8_t>& records, uint32_t id, const std::vector<uint8_t>& value)
{
  sen::ResizableBufferWriter writer(records);
  sen::OutputStream out(writer);
  out.writeUInt32(id);
  out.writeUInt32(static_cast<uint32_t>(value.size()));
  for (const auto byte: value)
  {
    out.writeUInt8(byte);
  }
}

/// Records by property id, as read back from a stream of them.
using Records = std::map<uint32_t, std::vector<uint8_t>>;

/// Reads a stream of records, adding each to records.
inline void readRecords(sen::Span<const uint8_t> bytes, Records& records)
{
  sen::InputStream in(bytes);
  while (!in.atEnd())
  {
    uint32_t id = 0U;
    uint32_t size = 0U;
    in.readUInt32(id);
    in.readUInt32(size);
    const auto* value = in.advance(size);
    records[id] = std::vector<uint8_t>(value, value + size);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
}

/// The change records an object writes, by transport mode.
struct Changes
{
  Records confirmed;  // NOLINT(misc-non-private-member-variables-in-classes)
  Records unicast;    // NOLINT(misc-non-private-member-variables-in-classes)
  Records multicast;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Protected members the kernel calls as a friend, reached through a derived class.
struct Access: sen::NativeObject
{
  static auto writeChanged() { return &Access::senImplWriteChangedPropertiesToStream; }
  static auto fieldGetter() { return &Access::senImplGetFieldValueGetter; }
  static auto writeStatic() { return &Access::senImplWriteStaticPropertiesToStream; }
  static auto writeDynamic() { return &Access::senImplWriteDynamicPropertiesToStream; }
};

/// The changes object writes, as the kernel collects them for remote subscribers.
inline Changes collectChanges(sen::NativeObject& object)
{
  constexpr std::size_t blockSize = 1024U;
  auto pool = sen::FixedMemoryBlockPool<blockSize>::make();
  std::vector<std::shared_ptr<sen::FixedMemoryBlock>> unicast;
  std::vector<std::shared_ptr<sen::FixedMemoryBlock>> multicast;
  auto provider = [&pool](std::vector<std::shared_ptr<sen::FixedMemoryBlock>>& blocks)
  {
    return [&pool, &blocks](std::size_t /*size*/)
    {
      blocks.push_back(pool->getBlockPtr());
      return sen::ResizableBufferWriter<sen::FixedMemoryBlock>(*blocks.back());
    };
  };

  std::vector<uint8_t> confirmedBytes;
  sen::ResizableBufferWriter writer(confirmedBytes);
  sen::OutputStream confirmed(writer);
  (object.*Access::writeChanged())(confirmed, provider(unicast), provider(multicast));

  Changes changes;
  readRecords(confirmedBytes, changes.confirmed);
  for (const auto& block: unicast)
  {
    readRecords(sen::makeConstSpan(block->data(), block->size()), changes.unicast);
  }
  for (const auto& block: multicast)
  {
    readRecords(sen::makeConstSpan(block->data(), block->size()), changes.multicast);
  }
  return changes;
}

/// The records object writes in one of its stream writers.
template <typename Writer>
Records collectRecords(const sen::Object& object, Writer member)
{
  std::vector<uint8_t> bytes;
  sen::ResizableBufferWriter writer(bytes);
  sen::OutputStream out(writer);
  (object.*member)(out);

  Records records;
  readRecords(bytes, records);
  return records;
}

/// The records of every property object writes.
inline Records collectAll(const sen::Object& object)
{
  std::vector<uint8_t> bytes;
  sen::ResizableBufferWriter writer(bytes);
  sen::OutputStream out(writer);
  sen::impl::writeAllPropertiesToStream(&object, out);

  Records records;
  readRecords(bytes, records);
  return records;
}

}  // namespace generic_test

#endif  // SEN_LIBS_KERNEL_TEST_UNIT_BUS_GENERIC_LOCAL_OBJECT_HELPERS_H
