// === replayed_object_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "replayed_object.h"
#include "replayer_test_helpers.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/sequence_traits.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/db/creation.h"
#include "sen/db/event.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/output.h"
#include "sen/db/property_change.h"
#include "sen/db/snapshot.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/detail/generic_local_object_proxy.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace replayer::test
{

class TestReplayedObject final: public sen::components::replayer::ReplayedObject
{
public:
  SEN_NOCOPY_NOMOVE(TestReplayedObject)
  using ReplayedObject::ReplayedObject;
  ~TestReplayedObject() override = default;

  using ReplayedObject::commit;
  using ReplayedObject::senImplComputeMaxReliableSerializedPropertySizeImpl;
  using ReplayedObject::senImplGetFieldValueGetter;
  using ReplayedObject::senImplRemoveTypedConnection;
  using ReplayedObject::senImplStreamCall;
  using ReplayedObject::senImplVariantCall;
  using ReplayedObject::senImplWriteChangedPropertiesToStream;
  using ReplayedObject::senImplWriteDynamicPropertiesToStream;
  using ReplayedObject::senImplWriteStaticPropertiesToStream;

  /// Writes all properties, as ReplayedObject does, and counts the calls.
  void senImplWriteAllPropertiesToStream(sen::OutputStream& out) const override
  {
    ++allWrites_;
    ReplayedObject::senImplWriteAllPropertiesToStream(out);
  }

  /// How many times the object has written all its properties.
  [[nodiscard]] std::size_t getAllWrites() const noexcept { return allWrites_; }

  /// Returns a property's encoding, as ReplayedObject does, and records which property was asked for.
  [[nodiscard]] sen::Span<const uint8_t> getPropertyEncoding(sen::MemberHash propertyId) const override
  {
    encodingsAsked_.push_back(propertyId.get());
    return ReplayedObject::getPropertyEncoding(propertyId);
  }

  /// The ids of the properties whose encodings were asked for, in order.
  [[nodiscard]] const std::vector<uint32_t>& getEncodingsAsked() const noexcept { return encodingsAsked_; }

private:
  mutable std::size_t allWrites_ = 0U;
  mutable std::vector<uint32_t> encodingsAsked_;
};

/// Gives the tests access to Object's protected property writers and senImplRemoveTypedConnection.
struct ObjectAccess: sen::Object
{
  static auto writeStatic() { return &ObjectAccess::senImplWriteStaticPropertiesToStream; }
  static auto writeDynamic() { return &ObjectAccess::senImplWriteDynamicPropertiesToStream; }
  static auto removeTypedConnection() { return &ObjectAccess::senImplRemoveTypedConnection; }
};

/// What object writes when it writes all its properties.
[[nodiscard]] std::vector<uint8_t> writeAll(const sen::Object& object)
{
  std::vector<uint8_t> bytes;
  sen::ResizableBufferWriter writer(bytes);
  sen::OutputStream out(writer);
  sen::impl::writeAllPropertiesToStream(&object, out);
  return bytes;
}

/// What object writes with one of its property writers.
template <typename Writer>
[[nodiscard]] std::vector<uint8_t> writeWith(const sen::Object& object, Writer writer)
{
  std::vector<uint8_t> bytes;
  sen::ResizableBufferWriter bufferWriter(bytes);
  sen::OutputStream out(bufferWriter);
  (object.*writer)(out);
  return bytes;
}

/// A replayed object's local proxy, recording the property and time of each change notification.
struct LocalProxyOf
{
  using Told = std::vector<std::pair<sen::MemberHash, sen::TimeStamp>>;
  using Proxy = sen::kernel::impl::GenericLocalObjectProxy;

  std::shared_ptr<Proxy> proxy;  // NOLINT(misc-non-private-member-variables-in-classes)
  Told told;                     // NOLINT(misc-non-private-member-variables-in-classes)

  explicit LocalProxyOf(sen::components::replayer::ReplayedObject* object)
    : proxy(std::dynamic_pointer_cast<Proxy>(object->senImplMakeLocalProxy("local")))
  {
    if (proxy == nullptr)
    {
      throw std::runtime_error("LocalProxyOf: a replayed object made no generic local proxy");
    }

    queue_.enable();
    for (const auto& prop: object->getAllProps())
    {
      guards_.push_back(proxy->onPropertyChangedUntyped(
        prop.get(),
        sen::EventCallback<sen::VarList>(&queue_,
                                         [this, id = prop->getId()](const sen::EventInfo& info, const sen::VarList&)
                                         { told.emplace_back(id, info.creationTime); })));
    }
  }

  /// Drains the proxy, as the subscriber's component does each cycle, and runs the queued callbacks.
  void drain()
  {
    proxy->drainInputs();
    while (queue_.executeAll())
    {
    }
  }

private:
  sen::impl::WorkQueue queue_ {256U, false};
  std::vector<sen::ConnectionGuard> guards_;
};

[[nodiscard]] const sen::Property* findPropertyByName(const TestReplayedObject& obj, std::string_view propertyName)
{
  const auto* property = obj.getClass().type()->searchPropertyByName(std::string(propertyName));
  if (!property)
  {
    throw std::runtime_error("findPropertyByName: property not found");
  }
  return property;
}

[[nodiscard]] const sen::Event* findEventByName(const TestReplayedObject& obj, std::string_view eventName)
{
  const auto* event = obj.getClass().type()->searchEventByName(std::string(eventName));
  if (!event)
  {
    throw std::runtime_error("findEventByName: event not found");
  }
  return event;
}

void executeQueue(sen::impl::WorkQueue& queue)
{
  while (queue.executeAll())
  {
  }
}

void executeObjectQueue(sen::NativeObject* object)
{
  auto* queue = sen::impl::getWorkQueue(object);
  if (!queue)
  {
    throw std::runtime_error("executeObjectQueue: object queue is null");
  }
  queue->enable();
  executeQueue(*queue);
}

[[nodiscard]] std::shared_ptr<TestReplayedObject> makeObject(const sen::db::Snapshot& snapshot,
                                                             sen::TimeStamp timeStamp)
{
  return std::make_shared<TestReplayedObject>(snapshot, timeStamp);
}

template <typename T>
void injectFlushCommit(TestReplayedObject& object, sen::TimeStamp timeStamp, const T& entry)
{
  object.inject(timeStamp, entry);
  object.flushPendingChanges(timeStamp);
  object.commit(timeStamp);
}

void flushCommit(TestReplayedObject& object, sen::TimeStamp timeStamp)
{
  object.flushPendingChanges(timeStamp);
  object.commit(timeStamp);
}

struct ReplayedObjectSetup
{
  TempDir tempDir;                                  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<DummyReplayObjImpl> object;       // NOLINT(misc-non-private-member-variables-in-classes)
  sen::kernel::TestComponent component;             // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<sen::kernel::TestKernel> kernel;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<sen::db::Input> input;            // NOLINT(misc-non-private-member-variables-in-classes)
  std::filesystem::path archivePath;                // NOLINT(misc-non-private-member-variables-in-classes)

  template <typename Writer>
  void writePropertyChange(sen::db::Output& output,
                           sen::TimeStamp timeStamp,
                           std::string_view propertyName,
                           Writer&& writer)
  {
    const auto* property = object->getClass().type()->searchPropertyByName(std::string(propertyName));
    if (!property)
    {
      throw std::runtime_error("writePropertyChange: property not found");
    }

    ::sen::kernel::Buffer buffer;
    sen::ResizableBufferWriter bufferWriter(buffer);
    sen::OutputStream out(bufferWriter);
    writer(out);
    output.propertyChange(timeStamp, object->getId(), property->getId(), std::move(buffer));
  }

  template <typename Writer>
  void writeEvent(sen::db::Output& output, sen::TimeStamp timeStamp, std::string_view eventName, Writer&& writer)
  {
    const auto* event = object->getClass().type()->searchEventByName(std::string(eventName));
    if (!event)
    {
      throw std::runtime_error("writeEvent: event not found");
    }

    ::sen::kernel::Buffer buffer;
    sen::ResizableBufferWriter bufferWriter(buffer);
    sen::OutputStream out(bufferWriter);
    writer(out);
    output.event(timeStamp, object->getId(), event->getId(), std::move(buffer));
  }

  ReplayedObjectSetup()
  {
    // register an object
    object = std::make_shared<DummyReplayObjImpl>("dummyObj", sen::VarMap {});
    component.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        auto source = api.getSource("local.test");
        source->add(object);
        return sen::kernel::done();
      });
    component.onRun([](auto& api) { return api.execLoop(std::chrono::seconds(1), []() {}); });
    kernel = std::make_unique<sen::kernel::TestKernel>(&component);
    kernel->step();

    // write archive to disk
    archivePath = makeArchivePath("replayed_obj_test", tempDir);
    auto settings = makeArchiveSettings("replayed_obj_test", tempDir);

    {
      sen::db::Output output(std::move(settings), []() {});

      auto info = makeObjectInfo(object);
      output.creation(kernel->getTime(), info, true);

      kernel->step();
      const auto entryTime = kernel->getTime();

      writePropertyChange(output,
                          entryTime,
                          "testProp",
                          [](sen::OutputStream& out) { sen::SerializationTraits<float64_t>::write(out, 12.3); });
      writePropertyChange(
        output, entryTime, "uniProp", [](sen::OutputStream& out) { sen::SerializationTraits<int32_t>::write(out, 1); });
      writePropertyChange(output,
                          entryTime,
                          "multiProp",
                          [](sen::OutputStream& out) { sen::SerializationTraits<int32_t>::write(out, 2); });
      writePropertyChange(output,
                          entryTime,
                          "enumProp",
                          [](sen::OutputStream& out) { sen::SerializationTraits<uint32_t>::write(out, 1U); });
      writePropertyChange(output,
                          entryTime,
                          "distProp",
                          [](sen::OutputStream& out) { sen::SerializationTraits<float32_t>::write(out, 2.5F); });
      writePropertyChange(output,
                          entryTime,
                          "seqProp",
                          [](sen::OutputStream& out)
                          {
                            std::vector<int32_t> values {1, 2, 3};
                            sen::SequenceTraitsBase<std::vector<int32_t>>::write(out, values);
                          });
      writePropertyChange(output, entryTime, "emptyStructProp", [](sen::OutputStream& out) { out.writeUInt8(0U); });

      writeEvent(output,
                 entryTime,
                 "testEvent",
                 [](sen::OutputStream& out)
                 {
                   sen::SerializationTraits<int32_t>::write(out, 99);
                   sen::SerializationTraits<std::string>::write(out, "hello");
                 });

      kernel->step();
      output.keyframe(kernel->getTime(), {info});
    }

    // open the archive for reading
    input = std::make_unique<sen::db::Input>(archivePath.string(), kernel->getTypes());
  }

  [[nodiscard]] sen::db::Snapshot findCreationSnapshot() const
  {
    auto cursor = input->begin();
    while (!cursor.atEnd())
    {
      ++cursor;
      if (cursor.atEnd())
      {
        break;
      }

      const auto& entry = cursor.get();
      if (std::holds_alternative<sen::db::Creation>(entry.payload))
      {
        return std::get<sen::db::Creation>(entry.payload).getSnapshot();
      }
    }
    throw std::runtime_error("no Creation entry found in the archive");
  }

  [[nodiscard]] sen::db::PropertyChange findPropertyChange() const { return findPropertyChangeNamed("testProp"); }

  [[nodiscard]] sen::db::PropertyChange findPropertyChangeNamed(const std::string& name) const
  {
    auto cursor = input->begin();
    while (!cursor.atEnd())
    {
      if (std::holds_alternative<sen::db::PropertyChange>(cursor.get().payload))
      {
        const auto& pc = std::get<sen::db::PropertyChange>(cursor.get().payload);
        if (pc.getProperty()->getName() == name)
        {
          return pc;
        }
      }
      ++cursor;
    }
    throw std::runtime_error("no PropertyChange entry found in the archive");
  }

  [[nodiscard]] sen::db::Event findEvent() const
  {
    auto cursor = input->begin();
    while (!cursor.atEnd())
    {
      if (std::holds_alternative<sen::db::Event>(cursor.get().payload))
      {
        return std::get<sen::db::Event>(cursor.get().payload);
      }
      ++cursor;
    }
    throw std::runtime_error("no Event entry found in the archive");
  }

  [[nodiscard]] sen::db::Snapshot findKeyframeSnapshot() const
  {
    auto cursor = input->begin();
    while (!cursor.atEnd())
    {
      ++cursor;
      if (cursor.atEnd())
      {
        break;
      }

      if (const auto& entry = cursor.get(); std::holds_alternative<sen::db::Keyframe>(entry.payload))
      {
        if (const auto& kf = std::get<sen::db::Keyframe>(entry.payload); !kf.getSnapshots().empty())
        {
          return kf.getSnapshots()[0];
        }
      }
    }
    throw std::runtime_error("no Keyframe with snapshots found in the archive");
  }
};

/// @test
/// Builds a replayed object from a creation snapshot carrying the recorded name, class, and
/// property values, with the construction time stamped as both the commit time and the
/// per-property time.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, ConstructionFromSnapshotSetsName)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findCreationSnapshot();
  const auto t0 = makeTime(3);
  auto obj = makeObject(snapshot, t0);

  EXPECT_EQ(obj->getName(), "dummyObj");
  ASSERT_NE(obj->getClass().type(), nullptr);
  EXPECT_EQ(obj->getClass().type()->getQualifiedName(), snapshot.getType().type()->getQualifiedName());
  EXPECT_EQ(obj->getLastCommitTime(), t0);
  EXPECT_FALSE(obj->getAllProps().empty());

  const auto* prop = findPropertyByName(*obj, "testProp");
  EXPECT_FALSE(obj->getPropertyUntyped(prop).isEmpty());
  EXPECT_EQ(obj->getPropertyLastTime(prop), t0);
}

/// @test
/// Keeps an injected property change invisible until flushPendingChanges and commit apply it,
/// after which the recorded 12.3 is readable and the commit and per-property times advance
/// with each applied entry.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, FlushAndCommitAppliesPropertyChange)
{
  ReplayedObjectSetup setup;
  auto creationSnapshot = setup.findCreationSnapshot();
  auto keyframeSnapshot = setup.findKeyframeSnapshot();
  const auto t0 = makeTime(1);
  auto obj = makeObject(creationSnapshot, t0);
  const auto* prop = findPropertyByName(*obj, "testProp");
  auto originalValue = obj->getPropertyUntyped(prop).getCopyAs<float64_t>();

  auto propChange = setup.findPropertyChange();
  const auto t1 = makeTime(5);
  obj->inject(t1, propChange);
  EXPECT_EQ(obj->getPropertyUntyped(prop).getCopyAs<float64_t>(), originalValue);

  flushCommit(*obj, t1);
  EXPECT_DOUBLE_EQ(obj->getPropertyUntyped(prop).getCopyAs<float64_t>(), 12.3);
  EXPECT_EQ(obj->getPropertyLastTime(prop), t1);

  const auto t2 = makeTime(9);
  injectFlushCommit(*obj, t2, keyframeSnapshot);
  EXPECT_EQ(obj->getLastCommitTime(), t2);

  const auto t3 = makeTime(12);
  injectFlushCommit(*obj, t3, propChange);
  EXPECT_EQ(obj->getLastCommitTime(), t3);
  EXPECT_EQ(obj->getPropertyLastTime(prop), t3);
  EXPECT_DOUBLE_EQ(obj->getPropertyUntyped(prop).getCopyAs<float64_t>(), 12.3);
}

/// @test
/// Applies a keyframe whose property value differs from the current one at the same serialized
/// size, as a seek does once a property change has been replayed.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, KeyframeAppliesSameSizedPropertyValue)
{
  ReplayedObjectSetup setup;
  auto creationSnapshot = setup.findCreationSnapshot();
  auto keyframeSnapshot = setup.findKeyframeSnapshot();
  const auto t0 = makeTime(1);
  auto obj = makeObject(creationSnapshot, t0);
  const auto* prop = findPropertyByName(*obj, "testProp");

  // the keyframe carries the recorded value; the property change below moves away from it
  const auto recordedValue = obj->getPropertyUntyped(prop).getCopyAs<float64_t>();
  ASSERT_NE(recordedValue, 12.3);

  const auto t1 = makeTime(5);
  injectFlushCommit(*obj, t1, setup.findPropertyChange());
  ASSERT_DOUBLE_EQ(obj->getPropertyUntyped(prop).getCopyAs<float64_t>(), 12.3);

  const auto t2 = makeTime(9);
  injectFlushCommit(*obj, t2, keyframeSnapshot);
  EXPECT_DOUBLE_EQ(obj->getPropertyUntyped(prop).getCopyAs<float64_t>(), recordedValue);
  EXPECT_EQ(obj->getPropertyLastTime(prop), t2);
}

/// @test
/// Throws a logic error when removing a typed connection on a replayed object, and raises
/// from the next-value property getter and setter as well.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, RemoveTypedConnectionThrowsLogicError)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findCreationSnapshot();
  auto obj = makeObject(snapshot, makeTime(1));
  const auto* prop = findPropertyByName(*obj, "testProp");

  EXPECT_THROW(obj->senImplRemoveTypedConnection(sen::ConnId {1}), std::logic_error);
  EXPECT_ANY_THROW(std::ignore = obj->getNextPropertyUntyped(prop));
  EXPECT_ANY_THROW(obj->setNextPropertyUntyped(prop, sen::Var(1.0)));
}

/// @test
/// Serializes a replayed object's state, the all-properties and dynamic streams come out
/// non-empty while the static stream stays empty, injected property changes apply and reach
/// at least one changed-properties destination, and an injected event drains through the work
/// queue.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, SerializesStateAndProcessesInjectedChanges)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findCreationSnapshot();
  const auto t0 = makeTime(1);
  auto obj = makeObject(snapshot, t0);

  {
    ::sen::kernel::Buffer allBuffer;
    sen::ResizableBufferWriter writer(allBuffer);
    sen::OutputStream out(writer);
    obj->senImplWriteAllPropertiesToStream(out);
    EXPECT_FALSE(allBuffer.empty());
  }
  {
    ::sen::kernel::Buffer staticBuffer;
    sen::ResizableBufferWriter writer(staticBuffer);
    sen::OutputStream out(writer);
    obj->senImplWriteStaticPropertiesToStream(out);
    EXPECT_TRUE(staticBuffer.empty());
  }
  {
    ::sen::kernel::Buffer dynamicBuffer;
    sen::ResizableBufferWriter writer(dynamicBuffer);
    sen::OutputStream out(writer);
    obj->senImplWriteDynamicPropertiesToStream(out);
    EXPECT_FALSE(dynamicBuffer.empty());
  }

  auto propChange = setup.findPropertyChangeNamed("testProp");
  const auto t1 = makeTime(4);
  injectFlushCommit(*obj, t1, propChange);
  auto* prop = findPropertyByName(*obj, "testProp");
  EXPECT_DOUBLE_EQ(obj->getPropertyUntyped(prop).getCopyAs<float64_t>(), 12.3);

  // Fresh object to isolate serialization state.
  auto serializationObj = makeObject(snapshot, t0);

  auto uniChange = setup.findPropertyChangeNamed("uniProp");
  auto multiChange = setup.findPropertyChangeNamed("multiProp");
  serializationObj->inject(t1, propChange);
  serializationObj->inject(t1, uniChange);
  serializationObj->inject(t1, multiChange);
  serializationObj->flushPendingChanges(t1);

  auto maxSize = serializationObj->senImplComputeMaxReliableSerializedPropertySizeImpl();
  EXPECT_GE(maxSize, 0U);

  auto pool = sen::FixedMemoryBlockPool<1024>::make();
  auto uniBlock = pool->getBlockPtr();
  auto multiBlock = pool->getBlockPtr();
  uint32_t uniCalls = 0;
  uint32_t multiCalls = 0;
  auto uniProvider = [&](std::size_t size)
  {
    ++uniCalls;
    uniBlock->resize(size);
    return sen::ResizableBufferWriter<sen::FixedMemoryBlock>(*uniBlock);
  };
  auto multiProvider = [&](std::size_t size)
  {
    ++multiCalls;
    multiBlock->resize(size);
    return sen::ResizableBufferWriter<sen::FixedMemoryBlock>(*multiBlock);
  };

  std::vector<uint8_t> confirmedBuf;
  sen::ResizableBufferWriter confirmedWriter(confirmedBuf);
  sen::OutputStream confirmedOut(confirmedWriter);
  serializationObj->senImplWriteChangedPropertiesToStream(confirmedOut, uniProvider, multiProvider);

  const auto serializedDestinations = uniCalls + multiCalls + static_cast<uint32_t>(!confirmedBuf.empty());
  EXPECT_GE(serializedDestinations, 1U);

  auto* objQueue = sen::impl::getWorkQueue(obj.get());
  ASSERT_NE(objQueue, nullptr);
  objQueue->enable();

  auto evt = setup.findEvent();
  const auto t2 = makeTime(7);
  obj->inject(t2, evt);
  obj->flushPendingChanges(t2);
  executeQueue(*objQueue);
  objQueue->clear();
  objQueue->disable();
}

/// @test
/// Invokes a property getter through both the stream and variant call paths, each callback
/// fires and the variant result is non-empty, while an unknown method id makes either path
/// throw.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, StreamCallToGetterSucceeds)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findCreationSnapshot();
  auto obj = makeObject(snapshot, makeTime(1));
  const auto* prop = findPropertyByName(*obj, "testProp");
  const auto methodId = prop->getGetterMethod().getId();

  std::vector<uint8_t> emptyBuf;
  sen::InputStream in(emptyBuf);

  bool streamCallbackInvoked = false;
  obj->senImplStreamCall(methodId,
                         in,
                         [&streamCallbackInvoked](sen::StreamCall&& streamWriter)
                         {
                           ::sen::kernel::Buffer resultBuf;
                           sen::ResizableBufferWriter writer(resultBuf);
                           sen::OutputStream out(writer);
                           streamWriter(out);
                           streamCallbackInvoked = true;
                         });
  EXPECT_TRUE(streamCallbackInvoked);

  bool variantCallbackInvoked = false;
  sen::Var result;
  obj->senImplVariantCall(methodId,
                          sen::VarList {},
                          [&variantCallbackInvoked, &result](sen::VariantCall&& variantWriter)
                          {
                            variantWriter(result);
                            variantCallbackInvoked = true;
                          });
  EXPECT_TRUE(variantCallbackInvoked);
  EXPECT_FALSE(result.isEmpty());

  sen::MemberHash wrongStreamId(1234);
  sen::MemberHash wrongVariantId(4321);
  EXPECT_ANY_THROW(obj->senImplStreamCall(wrongStreamId, in, [](sen::StreamCall&&) {}));
  EXPECT_ANY_THROW(obj->senImplVariantCall(wrongVariantId, sen::VarList {}, [](sen::VariantCall&&) {}));
}

/// @test
/// Resolves a field value getter for every supported scalar, string, and time property and
/// for nested struct and variant paths, accepts enum and distribution properties resolving
/// or throwing, while sequence properties and out-of-range struct or variant paths all throw.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, FieldGetterHandlesSupportedAndInvalidPaths)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findKeyframeSnapshot();
  auto obj = makeObject(snapshot, makeTime(1));

  auto expectFieldGetterWorks =
    [&](const std::string& name, std::vector<uint16_t> path = {}, bool runtimeErrorIsExpected = false)
  {
    const auto* property = obj->getClass().type()->searchPropertyByName(name);
    ASSERT_NE(property, nullptr);
    try
    {
      auto result = obj->senImplGetFieldValueGetter(property->getId(), sen::Span<uint16_t>(path.data(), path.size()));
      EXPECT_TRUE(result.getterFunc != nullptr);
    }
    catch (const std::runtime_error& err)
    {
      if (runtimeErrorIsExpected)
      {
        SUCCEED();
      }
      else
      {
        ADD_FAILURE() << "Unexpected runtime_error in expectFieldGetterWorks(" << name << "): " << err.what();
      }
    }
  };

  expectFieldGetterWorks("bProp");
  expectFieldGetterWorks("u8Prop");
  expectFieldGetterWorks("i16Prop");
  expectFieldGetterWorks("u16Prop");
  expectFieldGetterWorks("i32Prop");
  expectFieldGetterWorks("u32Prop");
  expectFieldGetterWorks("i64Prop");
  expectFieldGetterWorks("u64Prop");
  expectFieldGetterWorks("f32Prop");
  expectFieldGetterWorks("testProp");
  expectFieldGetterWorks("strProp");
  expectFieldGetterWorks("durProp");
  expectFieldGetterWorks("timeProp");
  expectFieldGetterWorks("enumProp", {}, true);
  expectFieldGetterWorks("distProp", {}, true);
  expectFieldGetterWorks("structProp", {0});
  expectFieldGetterWorks("structProp", {1, 0});

  const auto* variantProp = findPropertyByName(*obj, "variantProp");
  auto canReadVariantField = [&](uint16_t index)
  {
    std::vector<uint16_t> path {index};
    try
    {
      auto result =
        obj->senImplGetFieldValueGetter(variantProp->getId(), sen::Span<uint16_t>(path.data(), path.size()));
      return result.getterFunc != nullptr;
    }
    catch (...)
    {
      return false;
    }
  };
  EXPECT_TRUE(canReadVariantField(0) || canReadVariantField(1));

  EXPECT_ANY_THROW(static_cast<void>(
    obj->senImplGetFieldValueGetter(findPropertyByName(*obj, "seqProp")->getId(), sen::Span<uint16_t>())));

  std::vector<uint16_t> badStructPath {99U};
  EXPECT_ANY_THROW(static_cast<void>(obj->senImplGetFieldValueGetter(
    findPropertyByName(*obj, "structProp")->getId(), sen::Span<uint16_t>(badStructPath.data(), badStructPath.size()))));

  std::vector<uint16_t> badVariantPath {99U};
  EXPECT_ANY_THROW(static_cast<void>(
    obj->senImplGetFieldValueGetter(findPropertyByName(*obj, "variantProp")->getId(),
                                    sen::Span<uint16_t>(badVariantPath.data(), badVariantPath.size()))));

  std::vector<uint16_t> emptyStructPath {0U};
  EXPECT_ANY_THROW(static_cast<void>(
    obj->senImplGetFieldValueGetter(findPropertyByName(*obj, "emptyStructProp")->getId(),
                                    sen::Span<uint16_t>(emptyStructPath.data(), emptyStructPath.size()))));
}

/// @test
/// A local proxy notifies each replayed change once, with its recorded time, even for a repeated value.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, ProxyDrainInputsDetectsChanges)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findCreationSnapshot();
  const auto t0 = makeTime(1);
  auto obj = makeObject(snapshot, t0);
  ASSERT_TRUE(obj->getClass()->hasProxyMakers());

  LocalProxyOf local(obj.get());

  const auto* prop = findPropertyByName(*obj, "testProp");
  auto val = local.proxy->getPropertyUntyped(prop);
  EXPECT_FALSE(val.isEmpty());
  EXPECT_EQ(val.getCopyAs<float64_t>(), obj->getPropertyUntyped(prop).getCopyAs<float64_t>());

  local.drain();
  EXPECT_TRUE(local.told.empty());

  // recorded at t1, then flushed and committed later
  auto propChange = setup.findPropertyChangeNamed("testProp");
  const auto t1 = makeTime(5);
  obj->inject(t1, propChange);
  obj->flushPendingChanges(makeTime(6));
  obj->commit(makeTime(7));
  ASSERT_NE(obj->getLastCommitTime(), t1);

  local.drain();
  ASSERT_EQ(local.told.size(), 1U);
  EXPECT_EQ(local.told.front().first, prop->getId());
  EXPECT_EQ(local.told.front().second, t1);
  EXPECT_DOUBLE_EQ(local.proxy->getPropertyUntyped(prop).getCopyAs<float64_t>(), 12.3);

  local.told.clear();
  local.drain();
  EXPECT_TRUE(local.told.empty());

  const auto t2 = makeTime(9);
  obj->inject(t2, propChange);
  obj->flushPendingChanges(makeTime(10));
  obj->commit(makeTime(11));
  local.drain();
  ASSERT_EQ(local.told.size(), 1U);
  EXPECT_EQ(local.told.front().first, prop->getId());
  EXPECT_EQ(local.told.front().second, t2);
  EXPECT_DOUBLE_EQ(local.proxy->getPropertyUntyped(prop).getCopyAs<float64_t>(), 12.3);
}

/// @test
/// A local proxy writes the object's bytes as of its last drain; typed-connection removal is a no-op.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, ProxyWritesTheReplayedValues)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findCreationSnapshot();
  auto obj = makeObject(snapshot, makeTime(1));
  LocalProxyOf local(obj.get());

  EXPECT_EQ(writeAll(*local.proxy), writeAll(*obj));
  EXPECT_EQ(writeWith(*local.proxy, ObjectAccess::writeStatic()), writeWith(*obj, ObjectAccess::writeStatic()));
  EXPECT_EQ(writeWith(*local.proxy, ObjectAccess::writeDynamic()), writeWith(*obj, ObjectAccess::writeDynamic()));

  injectFlushCommit(*obj, makeTime(5), setup.findPropertyChangeNamed("testProp"));
  const auto before = writeAll(*local.proxy);
  EXPECT_NE(writeAll(*obj), before);

  local.drain();
  EXPECT_EQ(writeAll(*local.proxy), writeAll(*obj));

  EXPECT_NO_THROW(((*local.proxy).*ObjectAccess::removeTypedConnection())(sen::ConnId {1}));
}

/// @test
/// A replayed object writes all its properties only once, when its local proxy is made.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, ProxyDoesNotRewriteAnUnchangedObject)
{
  ReplayedObjectSetup setup;
  auto obj = makeObject(setup.findCreationSnapshot(), makeTime(1));
  LocalProxyOf local(obj.get());
  ASSERT_EQ(obj->getAllWrites(), 1U) << "the proxy takes the object's state when it is made";

  local.drain();
  obj->commit(makeTime(2));
  local.drain();
  EXPECT_EQ(obj->getAllWrites(), 1U);
  EXPECT_TRUE(local.told.empty());

  injectFlushCommit(*obj, makeTime(5), setup.findPropertyChangeNamed("testProp"));
  local.drain();
  EXPECT_EQ(obj->getAllWrites(), 1U);
  EXPECT_EQ(local.told.size(), 1U);

  local.drain();
  EXPECT_EQ(obj->getAllWrites(), 1U);
}

/// @test
/// A drain reads each property's encoding once and notifies only those whose recorded time moved.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, ProxyTellsOnlyOfPropertiesWhoseTimeMoved)
{
  ReplayedObjectSetup setup;
  auto obj = makeObject(setup.findCreationSnapshot(), makeTime(1));
  LocalProxyOf local(obj.get());
  ASSERT_TRUE(obj->getEncodingsAsked().empty()) << "the proxy is made from a full write";

  std::vector<uint32_t> everyProperty;
  for (const auto& prop: obj->getAllProps())
  {
    everyProperty.push_back(prop->getId().get());
  }

  const auto* testProp = findPropertyByName(*obj, "testProp");
  const auto* uniProp = findPropertyByName(*obj, "uniProp");
  const auto t1 = makeTime(5);
  obj->inject(t1, setup.findPropertyChangeNamed("testProp"));
  obj->inject(t1, setup.findPropertyChangeNamed("uniProp"));
  obj->flushPendingChanges(t1);
  obj->commit(t1);
  local.drain();

  EXPECT_EQ(obj->getEncodingsAsked(), everyProperty);
  ASSERT_EQ(local.told.size(), 2U);
  EXPECT_EQ(local.told[0].first, testProp->getId());
  EXPECT_EQ(local.told[1].first, uniProp->getId());
  EXPECT_DOUBLE_EQ(local.proxy->getPropertyUntyped(testProp).getCopyAs<float64_t>(), 12.3);
  EXPECT_EQ(local.proxy->getPropertyUntyped(uniProp).getCopyAs<int32_t>(), 1);

  local.told.clear();
  local.drain();
  EXPECT_EQ(obj->getEncodingsAsked().size(), 2U * everyProperty.size()) << "compared again";
  EXPECT_TRUE(local.told.empty()) << "no time moved since";
  EXPECT_EQ(obj->getAllWrites(), 1U);
}

/// @test
/// A value committed without its recorded time moving is read at the next drain but not notified.
/// @requirements(SEN-364)
TEST(ReplayedObjectTest, ProxyReadsAValueChangedUnderAnUnmovedTime)
{
  ReplayedObjectSetup setup;
  auto obj = makeObject(setup.findCreationSnapshot(), makeTime(1));
  LocalProxyOf local(obj.get());
  const auto* prop = findPropertyByName(*obj, "testProp");
  const auto recorded = obj->getPropertyUntyped(prop).getCopyAs<float64_t>();
  ASSERT_NE(recorded, 12.3);

  const auto t1 = makeTime(5);
  injectFlushCommit(*obj, t1, setup.findPropertyChangeNamed("testProp"));
  local.drain();
  ASSERT_DOUBLE_EQ(local.proxy->getPropertyUntyped(prop).getCopyAs<float64_t>(), 12.3);
  local.told.clear();

  // a keyframe at the change's own time, holding the value from before it
  injectFlushCommit(*obj, t1, setup.findKeyframeSnapshot());
  ASSERT_EQ(obj->getPropertyLastTime(prop), t1);
  ASSERT_DOUBLE_EQ(obj->getPropertyUntyped(prop).getCopyAs<float64_t>(), recorded);

  local.drain();
  EXPECT_DOUBLE_EQ(local.proxy->getPropertyUntyped(prop).getCopyAs<float64_t>(), recorded);
  for (const auto& told: local.told)
  {
    EXPECT_NE(told.first, prop->getId()) << "told of a property whose time did not move";
  }
}

}  // namespace replayer::test
