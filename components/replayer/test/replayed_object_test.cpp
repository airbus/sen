// === replayed_object_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "replayed_object.h"
#include "replayed_object_proxy.h"
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
#include "sen/db/creation.h"
#include "sen/db/event.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/output.h"
#include "sen/db/property_change.h"
#include "sen/db/snapshot.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
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
  using ReplayedObject::senImplWriteAllPropertiesToStream;
  using ReplayedObject::senImplWriteChangedPropertiesToStream;
  using ReplayedObject::senImplWriteDynamicPropertiesToStream;
  using ReplayedObject::senImplWriteStaticPropertiesToStream;
};

class TestReplayedObjectProxy final: public sen::components::replayer::ReplayedObjectProxy
{
public:
  SEN_NOCOPY_NOMOVE(TestReplayedObjectProxy)
  TestReplayedObjectProxy(sen::components::replayer::ReplayedObject* owner, std::string_view localPrefix)
    : ReplayedObjectProxy(owner, localPrefix)
  {
    eventQueue_.enable();

    for (const auto& prop: owner->getAllProps())
    {
      guards.push_back(onPropertyChangedUntyped(
        prop.get(),
        sen::EventCallback<sen::VarList>(&eventQueue_,
                                         [this, id = prop->getId()](const sen::EventInfo&, const sen::VarList&)
                                         { emittedEvents.push_back(id); })));
    }
  }

  ~TestReplayedObjectProxy() override = default;

  using ReplayedObjectProxy::drainInputsImpl;
  using ReplayedObjectProxy::senImplGetPropertyImpl;
  using ReplayedObjectProxy::senImplRemoveTypedConnection;
  using ReplayedObjectProxy::senImplWriteAllPropertiesToStream;
  using ReplayedObjectProxy::senImplWriteDynamicPropertiesToStream;
  using ReplayedObjectProxy::senImplWriteStaticPropertiesToStream;

  void executeCallbacks()
  {
    while (eventQueue_.executeAll())
    {
    }
  }
  // intercept events for validation
  std::vector<sen::MemberHash> emittedEvents;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<sen::ConnectionGuard> guards;    // NOLINT(misc-non-private-member-variables-in-classes)

private:
  sen::impl::WorkQueue eventQueue_ {256U, false};
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
/// Builds a replayed object from a snapshot and preserves its initial state
/// requirements(SEN-364)
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
/// Applies injected changes only after flush and commit
/// requirements(SEN-364)
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
/// Rejects mutable operations on replayed objects
/// requirements(SEN-364)
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
/// Serializes replayed state and processes injected changes
/// requirements(SEN-364)
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
  auto uniProvider = [&](uint32_t size)
  {
    ++uniCalls;
    uniBlock->resize(size);
    return sen::ResizableBufferWriter<sen::FixedMemoryBlock>(*uniBlock);
  };
  auto multiProvider = [&](uint32_t size)
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
/// Allows getter calls and rejects unknown method ids
/// requirements(SEN-364)
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
/// Resolves supported field getters and rejects invalid paths
/// requirements(SEN-364)
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
/// Proxy mirrors committed changes and blocks invalid native operations
/// requirements(SEN-364)
TEST(ReplayedObjectTest, ProxyDrainInputsDetectsChanges)
{
  ReplayedObjectSetup setup;
  auto snapshot = setup.findCreationSnapshot();
  const auto t0 = makeTime(1);
  auto obj = makeObject(snapshot, t0);
  TestReplayedObjectProxy proxy(obj.get(), "local");

  const auto* prop = findPropertyByName(*obj, "testProp");
  auto val = proxy.senImplGetPropertyImpl(prop->getId());
  EXPECT_FALSE(val.isEmpty());
  EXPECT_EQ(val.getCopyAs<float64_t>(), obj->getPropertyUntyped(prop).getCopyAs<float64_t>());

  proxy.drainInputsImpl(t0);
  proxy.executeCallbacks();
  EXPECT_TRUE(proxy.emittedEvents.empty());

  auto propChange = setup.findPropertyChangeNamed("testProp");
  const auto t1 = makeTime(5);
  injectFlushCommit(*obj, t1, propChange);

  proxy.drainInputsImpl(t1);
  proxy.executeCallbacks();
  ASSERT_EQ(proxy.emittedEvents.size(), 1);
  EXPECT_EQ(proxy.emittedEvents.front(), prop->getId());

  proxy.emittedEvents.clear();
  proxy.drainInputsImpl(t1);
  proxy.executeCallbacks();
  EXPECT_TRUE(proxy.emittedEvents.empty());

  std::vector<uint8_t> buf;
  sen::ResizableBufferWriter writer(buf);
  sen::OutputStream out(writer);
  EXPECT_THROW(proxy.senImplWriteAllPropertiesToStream(out), std::logic_error);
  EXPECT_THROW(proxy.senImplWriteStaticPropertiesToStream(out), std::logic_error);
  EXPECT_THROW(proxy.senImplWriteDynamicPropertiesToStream(out), std::logic_error);
  EXPECT_THROW(proxy.senImplRemoveTypedConnection(sen::ConnId {1}), std::logic_error);
}

}  // namespace replayer::test
