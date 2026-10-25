// === generic_remote_object_test.cpp ==================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Tests GenericRemoteObject reading a class that the writer and the reader define differently.

// sen
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/detail/generic_remote_object.h"

// gtest
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using sen::kernel::impl::GenericRemoteObject;

namespace
{

/// A class with two properties: val, of the given type, then next, a u32.
sen::TypeHandle<sen::ClassType> makeClass(sen::ConstTypeHandle<> valType)
{
  sen::ClassSpec spec {"Changed",
                       "ns.Changed",
                       "",
                       {{"val", "", std::move(valType)}, {"next", "", sen::UInt32Type::get()}},
                       {},
                       {},
                       std::nullopt,
                       {},
                       false,
                       {},
                       {}};
  return sen::ClassType::make(spec);
}

/// A class with one event, happened, with the given arguments in that order.
sen::TypeHandle<sen::ClassType> makeEmitter(std::vector<sen::Arg> args)
{
  const sen::EventSpec event {{"happened", "", std::move(args)}};
  sen::ClassSpec spec {"Emitter", "ns.Emitter", "", {}, {}, {event}, std::nullopt, {}, false, {}, {}};
  return sen::ClassType::make(spec);
}

/// Gives the test access to the protected RemoteObject::eventReceived.
struct EventAccess: sen::impl::RemoteObject
{
  static auto received() { return &EventAccess::eventReceived; }
};

/// Delivers written as the arguments of happened, and returns what an untyped subscriber receives.
std::optional<sen::VarList> deliver(GenericRemoteObject& object,
                                    const sen::ClassType& reader,
                                    const std::vector<uint8_t>& written)
{
  sen::impl::WorkQueue queue {16U, false};
  queue.enable();

  std::optional<sen::VarList> given;
  auto keep = [&given](const sen::EventInfo& /*info*/, const sen::VarList& args) { given = args; };
  const auto* happened = reader.searchEventByName("happened");
  auto guard = object.onEventUntyped(happened, sen::EventCallback<sen::VarList>(&queue, keep));

  (object.*EventAccess::received())(happened->getId(), sen::TimeStamp {}, sen::Span<const uint8_t>(written));
  while (queue.executeAll())
  {
  }
  return given;
}

/// The info for a proxy of class reader over an object of class writer.
sen::impl::RemoteObjectInfo infoFor(const sen::TypeHandle<sen::ClassType>& reader,
                                    const sen::TypeHandle<sen::ClassType>& writer)
{
  return {
    reader, "remote", sen::ObjectId {1U}, nullptr, {}, "test.remote", nullptr, sen::ObjectOwnerId {1U}, {}, writer};
}

/// An object's state as the writer sends it: per property, its id, its size and its value.
std::vector<uint8_t> writerState(const sen::ClassType& writer, uint32_t val, uint32_t next)
{
  std::vector<uint8_t> state;
  sen::ResizableBufferWriter bufferWriter(state);
  sen::OutputStream out(bufferWriter);

  out.writeUInt32(writer.searchPropertyByName("val")->getId().get());
  out.writeUInt32(sizeof(uint32_t));
  out.writeUInt32(val);

  out.writeUInt32(writer.searchPropertyByName("next")->getId().get());
  out.writeUInt32(sizeof(uint32_t));
  out.writeUInt32(next);

  return state;
}

}  // namespace

/// @test
/// A u32 written for a u8 property is read as a u32 and converted; the next property reads correctly.
TEST(AGenericRemoteObject, readsAChangedPropertyThroughTheWriterSchema)
{
  const auto reader = makeClass(sen::UInt8Type::get());
  const auto writer = makeClass(sen::UInt32Type::get());
  ASSERT_NE(reader->getHash(), writer->getHash());

  const auto object = std::make_shared<GenericRemoteObject>(infoFor(reader, writer));
  object->initializeState(writerState(*writer, 200U, 7U), sen::TimeStamp {});

  const auto val = object->getPropertyUntyped(reader->searchPropertyByName("val"));
  EXPECT_TRUE(val.holds<uint8_t>());
  EXPECT_EQ(sen::getCopyAs<uint32_t>(val), 200U);

  const auto next = object->getPropertyUntyped(reader->searchPropertyByName("next"));
  EXPECT_EQ(sen::getCopyAs<uint32_t>(next), 7U);
}

/// @test
/// With no writer schema, the stream is read with the reader's class.
TEST(AGenericRemoteObject, readsAnUnchangedClassAsItIs)
{
  const auto type = makeClass(sen::UInt32Type::get());

  auto info = infoFor(type, type);
  info.writerSchema = std::nullopt;
  const auto object = std::make_shared<GenericRemoteObject>(std::move(info));
  object->initializeState(writerState(*type, 200U, 7U), sen::TimeStamp {});

  EXPECT_EQ(sen::getCopyAs<uint32_t>(object->getPropertyUntyped(type->searchPropertyByName("val"))), 200U);
  EXPECT_EQ(sen::getCopyAs<uint32_t>(object->getPropertyUntyped(type->searchPropertyByName("next"))), 7U);
}

/// @test
/// An event's arguments are matched by name, whatever their order, and converted to the reader's.
TEST(AGenericRemoteObject, readsAChangedEventThroughTheWriterSchema)
{
  const auto reader = makeEmitter({{"level", "", sen::UInt32Type::get()}, {"tag", "", sen::UInt8Type::get()}});
  const auto writer = makeEmitter({{"tag", "", sen::UInt8Type::get()}, {"level", "", sen::UInt16Type::get()}});
  ASSERT_NE(reader->getHash(), writer->getHash());
  const auto object = std::make_shared<GenericRemoteObject>(infoFor(reader, writer));

  std::vector<uint8_t> written;
  {
    sen::ResizableBufferWriter bufferWriter(written);
    sen::OutputStream out(bufferWriter);
    out.writeUInt8(7U);
    out.writeUInt16(500U);
  }

  const auto given = deliver(*object, *reader, written);
  ASSERT_TRUE(given.has_value()) << "the event was dropped";
  ASSERT_EQ(given->size(), 2U);
  EXPECT_TRUE(given->at(0).holds<uint32_t>());
  EXPECT_EQ(sen::getCopyAs<uint32_t>(given->at(0)), 500U);
  EXPECT_EQ(sen::getCopyAs<uint32_t>(given->at(1)), 7U);
}

/// @test
/// An event with an argument the writer's event lacks is dropped.
TEST(AGenericRemoteObject, dropsAChangedEventWhoseArgumentHasNoNamesake)
{
  const auto reader = makeEmitter({{"level", "", sen::UInt32Type::get()}, {"tag", "", sen::UInt8Type::get()}});
  const auto writer = makeEmitter({{"level", "", sen::UInt32Type::get()}, {"label", "", sen::UInt8Type::get()}});
  ASSERT_NE(reader->getHash(), writer->getHash());
  const auto object = std::make_shared<GenericRemoteObject>(infoFor(reader, writer));

  std::vector<uint8_t> written;
  {
    sen::ResizableBufferWriter bufferWriter(written);
    sen::OutputStream out(bufferWriter);
    out.writeUInt32(500U);
    out.writeUInt8(7U);
  }

  EXPECT_FALSE(deliver(*object, *reader, written).has_value());
}
