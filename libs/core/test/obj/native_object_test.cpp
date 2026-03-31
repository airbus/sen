// === native_object_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/detail/property_flags.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

class Runner
{
public:
  static void setRegistrationTime(NativeObject* obj, const TimeStamp timeStamp) { obj->setRegistrationTime(timeStamp); }
  static TimeStamp getRegistrationTime(const NativeObject* obj) { return obj->getRegistrationTime(); }
  static void setQueues(NativeObject* obj,
                        sen::impl::WorkQueue* workQueue,
                        sen::impl::SerializableEventQueue* eventQueue)
  {
    obj->setQueues(workQueue, eventQueue);
  }
};

}  // namespace sen::kernel::impl

namespace
{

class ExceptionThrowingObject final: public example_class::ExampleClassBase
{
public:
  using ExampleClassBase::ExampleClassBase;

  void throwError() const
  {
    std::ignore = this->getName();
    throw std::runtime_error("Test exception");
  }

  void throwErrorConst() const
  {
    std::ignore = this->getName();
    throw std::runtime_error("Test const exception");
  }

  int throwNonVoid() const
  {
    std::ignore = this->getName();
    throw std::runtime_error("Non-void err");
  }

  void testAsyncCall(sen::MethodCallback<void>&& cb)
  {
    senImplAsyncCall(this, std::move(cb), &ExceptionThrowingObject::throwError, false);
  }

  void testAsyncCallConst(sen::MethodCallback<void>&& cb) const
  {
    senImplAsyncCall(this, std::move(cb), &ExceptionThrowingObject::throwErrorConst, false);
  }

  void testAsyncCallNonVoid(sen::MethodCallback<int>&& cb) const
  {
    senImplAsyncCall(this, std::move(cb), &ExceptionThrowingObject::throwNonVoid, false);
  }
};

class DeferredObject final: public example_class::ExampleClassBase
{
public:
  using ExampleClassBase::ExampleClassBase;

  void deferredMethod(std::promise<int> promise) const
  {
    std::ignore = this->getName();
    promise.set_value(42);
  }

  void deferredMethodVoid(std::promise<void> promise) const
  {
    std::ignore = this->getName();
    promise.set_value();
  }

  void deferredMethodException(std::promise<int> /*promise*/) const
  {
    std::ignore = this->getName();
    throw std::runtime_error("Deferred execution failed immediately");
  }

  void testDeferredCall(sen::MethodCallback<int>&& cb)
  {
    senImplAsyncDeferredCall(this, std::move(cb), &DeferredObject::deferredMethod, false);
  }

  void testDeferredCallVoid(sen::MethodCallback<void>&& cb)
  {
    senImplAsyncDeferredCall(this, std::move(cb), &DeferredObject::deferredMethodVoid, false);
  }

  void testDeferredCallException(sen::MethodCallback<int>&& cb)
  {
    senImplAsyncDeferredCall(this, std::move(cb), &DeferredObject::deferredMethodException, false);
  }
};

class ReturnObject final: public example_class::ExampleClassBase
{
public:
  using ExampleClassBase::ExampleClassBase;

  sen::Duration getDuration() const
  {
    std::ignore = this->getName();
    return sen::Duration {42};
  }

  void testAsyncReturn(sen::MethodCallback<sen::Duration>&& cb) const
  {
    senImplAsyncCall(this, std::move(cb), &ReturnObject::getDuration, false);
  }
};

class ConstReturnObject final: public example_class::ExampleClassBase
{
public:
  using ExampleClassBase::ExampleClassBase;

  int getValueConst() const
  {
    std::ignore = this->getName();
    return 100;
  }

  void testAsyncConst(sen::MethodCallback<int>&& cb) const
  {
    senImplAsyncCall(this, std::move(cb), &ConstReturnObject::getValueConst, false);
  }
};

class VariantThrowingObject final: public example_class::ExampleClassBase
{
public:
  using ExampleClassBase::ExampleClassBase;
  void senImplVariantCall(const sen::MemberHash methodId,
                          const sen::VarList& args,
                          sen::VariantCallForwarder&& fwd) override
  {
    std::ignore = methodId;
    std::ignore = args;
    fwd([](sen::Var&) { throw std::runtime_error("Variant error"); });
  }
};

struct PublicNativeObject final: example_class::ExampleClassBase
{
  using ExampleClassBase::commit;
  using ExampleClassBase::ExampleClassBase;
  using ExampleClassBase::senImplWriteChangedPropertiesToStream;
  using NativeObject::senImplEventEmitted;
};

}  // namespace

/// @test
/// Verifies the default implementations of virtual pipeline hooks and type conversion inside NativeObject
/// @requirements(SEN-351)
TEST(NativeObject, DefaultVirtualMethods)
{
  PublicNativeObject obj("TestObj");

  EXPECT_FALSE(obj.needsPreDrainOrPreCommit());

  EXPECT_NO_THROW({
    obj.preDrain();
    obj.preCommit();
  });

  const PublicNativeObject* constObj = &obj;
  EXPECT_EQ(constObj->asNativeObject(), &obj);
  EXPECT_EQ(obj.asNativeObject(), &obj);
}

/// @test
/// Confirms the registration time can be correctly set and retrieved by the execution environment
/// @requirements(SEN-351)
TEST(NativeObject, RegistrationTime)
{
  PublicNativeObject obj("TestObj");

  sen::kernel::impl::Runner::setRegistrationTime(&obj, sen::TimeStamp {12345});
  EXPECT_EQ(sen::kernel::impl::Runner::getRegistrationTime(&obj).sinceEpoch().getNanoseconds(), 12345);
}

/// @test
/// Validates untyped property change bindings attach properly and trigger on execution
/// @requirements(SEN-574)
TEST(NativeObject, OnPropertyChangedUntyped)
{
  const auto obj = std::make_shared<PublicNativeObject>("TestObj");
  sen::impl::getWorkQueue(obj.get())->enable();

  const auto* prop = obj->getClass()->searchPropertyByName("prop1");

  bool invoked = false;
  auto cb = sen::EventCallback<sen::VarList>(sen::impl::getWorkQueue(obj.get()),
                                             [&](const sen::EventInfo&, sen::VarList) { invoked = true; });

  auto guard = obj->onPropertyChangedUntyped(prop, std::move(cb));

  obj->setNextProp1("updated");
  obj->commit(sen::TimeStamp {100});

  while (sen::impl::getWorkQueue(obj.get())->executeAll())
  {
  }

  EXPECT_TRUE(invoked);
}

/// @test
/// Verifies that untyped event subscriptions securely bind and resolve upon correct payload dispatch
/// @requirements(SEN-574)
TEST(NativeObject, UntypedEventSubscription)
{
  const auto obj = std::make_shared<PublicNativeObject>("TestObj");
  sen::impl::getWorkQueue(obj.get())->enable();
  const auto* ev = obj->getClass()->searchEventByName("prop1Changed");

  bool invoked = false;
  auto cb = sen::EventCallback<sen::VarList>(sen::impl::getWorkQueue(obj.get()),
                                             [&](const sen::EventInfo&, sen::VarList) { invoked = true; });

  auto guard = obj->onEventUntyped(ev, std::move(cb));

  obj->senImplEventEmitted(ev->getId(), [] { return sen::VarList {}; }, sen::EventInfo {sen::TimeStamp {100}});

  while (sen::impl::getWorkQueue(obj.get())->executeAll())
  {
  }

  EXPECT_TRUE(invoked);
}

/// @test
/// Validates that an untyped connection drops its routing callback faithfully when out of scope
/// @requirements(SEN-574)
TEST(NativeObject, UntypedConnectionRemoval)
{
  const auto obj = std::make_shared<PublicNativeObject>("TestObj");
  sen::impl::getWorkQueue(obj.get())->enable();
  const auto* prop = obj->getClass()->searchPropertyByName("prop1");

  bool invoked = false;

  {
    auto cb = sen::EventCallback<sen::VarList>(sen::impl::getWorkQueue(obj.get()),
                                               [&](const sen::EventInfo&, sen::VarList) { invoked = true; });
    auto guard = obj->onPropertyChangedUntyped(prop, std::move(cb));
  }

  obj->setNextProp1("updated");
  obj->commit(sen::TimeStamp {100});

  while (sen::impl::getWorkQueue(obj.get())->executeAll())
  {
  }

  EXPECT_FALSE(invoked);
}

/// @test
/// Ensures passing an invalidated callback safely aborts connection and maintains clean state
/// @requirements(SEN-574)
TEST(NativeObject, InvalidatedCallbackConnection)
{
  const auto obj = std::make_shared<PublicNativeObject>("TestObj");
  const auto* prop = obj->getClass()->searchPropertyByName("prop1");

  sen::EventCallback<sen::VarList> emptyCb;
  auto guard = obj->onPropertyChangedUntyped(prop, std::move(emptyCb));

  SUCCEED();
}

/// @test
/// Ensures that an invalidated callback on untyped event subscriptions safely exits early
/// @requirements(SEN-574)
TEST(NativeObject, InvalidatedCallbackConnectionEvent)
{
  const auto obj = std::make_shared<PublicNativeObject>("TestObj");
  const auto* ev = obj->getClass()->searchEventByName("prop1Changed");

  sen::EventCallback<sen::VarList> emptyCb;
  auto guard = obj->onEventUntyped(ev, std::move(emptyCb));

  SUCCEED();
}

/// @test
/// Confirms runtime untyped reads and writes adapt cleanly into native internal structures
/// @requirements(SEN-351)
TEST(NativeObject, UntypedPropertyGetSet)
{
  PublicNativeObject obj("TestObj");
  const auto* prop = obj.getClass()->searchPropertyByName("prop1");

  obj.setNextPropertyUntyped(prop, sen::Var(std::string("untyped_val")));
  EXPECT_EQ(obj.getNextPropertyUntyped(prop).get<std::string>(), "untyped_val");

  obj.commit(sen::TimeStamp {100});
  EXPECT_EQ(obj.getPropertyUntyped(prop).get<std::string>(), "untyped_val");
}

/// @test
/// Ensures async calls gracefully short-circuit and return an error if the host object is destroyed
/// before the queued work gets executed
/// @requirements(SEN-351)
TEST(NativeObject, AsyncCallObjectDestroyed)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();

  bool errorHit = false;

  {
    auto callBack =
      sen::MethodCallback<sen::Var>(&queue, [&](const auto&, const auto& res) { errorHit = res.isError(); });
    const auto obj = std::make_shared<PublicNativeObject>("TestObj");
    sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);

    const auto* method = obj->getClass()->searchMethodByName("setNextProp1");
    obj->invokeUntyped(method, {sen::Var(std::string("val"))}, std::move(callBack));
  }

  while (queue.executeAll())
  {
  }

  EXPECT_TRUE(errorHit);
}

/// @test
/// Ensures non-const async calls gracefully short-circuit if host object is destroyed
/// @requirements(SEN-351)
TEST(NativeObject, AsyncCallNonConstObjectDestroyed)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();

  bool errorHit = false;

  {
    auto callBack = sen::MethodCallback<void>(&queue, [&](const auto&, const auto& res) { errorHit = res.isError(); });
    const auto obj = std::make_shared<ExceptionThrowingObject>("ThrowObj");
    sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);

    obj->testAsyncCall(std::move(callBack));
  }

  while (queue.executeAll())
  {
  }

  EXPECT_TRUE(errorHit);
}

/// @test
/// Ensures const async calls effectively cancel execution when the parent proxy context has been destroyed
/// @requirements(SEN-351)
TEST(NativeObject, AsyncCallObjectDestroyedConst)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();

  bool errorHit = false;

  {
    auto callBack = sen::MethodCallback<int>(&queue, [&](const auto&, const auto& res) { errorHit = res.isError(); });
    const auto obj = std::make_shared<ConstReturnObject>("ConstObj");
    sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);

    obj->testAsyncConst(std::move(callBack));
  }

  while (queue.executeAll())
  {
  }

  EXPECT_TRUE(errorHit);
}

/// @test
/// Ensures template async calls gracefully short-circuit if host object is destroyed
/// @requirements(SEN-351)
TEST(NativeObject, AsyncCallTemplateObjectDestroyed)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();

  bool errorHit = false;

  {
    auto callBack =
      sen::MethodCallback<sen::Duration>(&queue, [&](const auto&, const auto& res) { errorHit = res.isError(); });
    const auto obj = std::make_shared<ReturnObject>("TestObj");
    sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);

    obj->testAsyncReturn(std::move(callBack));
  }

  while (queue.executeAll())
  {
  }

  EXPECT_TRUE(errorHit);
}

/// @test
/// Ensures deferred async calls gracefully short-circuit if host object is destroyed
/// @requirements(SEN-351)
TEST(NativeObject, DeferredCallObjectDestroyed)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();

  bool errorHit = false;

  {
    auto callBack = sen::MethodCallback<int>(&queue, [&](const auto&, const auto& res) { errorHit = res.isError(); });
    const auto obj = std::make_shared<DeferredObject>("TestObj");
    sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);

    obj->testDeferredCall(std::move(callBack));
  }

  while (queue.executeAll())
  {
  }

  EXPECT_TRUE(errorHit);
}

/// @test
/// Verifies const asynchronous method calls execute successfully when object is alive
/// @requirements(SEN-351)
TEST(NativeObject, AsyncCallConstSuccess)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();
  const auto obj = std::make_shared<ConstReturnObject>("ConstObj");
  sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);

  int val = 0;
  auto cb = sen::MethodCallback<int>(&queue,
                                     [&](const auto&, const auto& res)
                                     {
                                       if (res.isOk())
                                       {
                                         val = res.getValue();
                                       }
                                     });

  obj->testAsyncConst(std::move(cb));

  while (queue.executeAll())
  {
  }

  EXPECT_EQ(val, 100);
}

/// @test
/// Confirms methods natively returning non-void standard values invoke back effectively
/// @requirements(SEN-351)
TEST(NativeObject, ExecuteCallWithArgsNonVoid)
{
  const auto obj = std::make_shared<ReturnObject>("RetObj");
  sen::impl::getWorkQueue(obj.get())->enable();

  sen::Duration val;
  auto cb = sen::MethodCallback<sen::Duration>(sen::impl::getWorkQueue(obj.get()),
                                               [&](const auto&, const auto& res)
                                               {
                                                 if (res.isOk())
                                                 {
                                                   val = res.getValue();
                                                 }
                                               });
  obj->testAsyncReturn(std::move(cb));

  while (sen::impl::getWorkQueue(obj.get())->executeAll())
  {
  }

  EXPECT_EQ(val.getNanoseconds(), 42);
}

/// @test
/// Confirms exceptions thrown from natively executed methods are safely caught and bubbled up as errors
/// @requirements(SEN-351)
TEST(NativeObject, ExecuteCallWithArgsException)
{
  const auto obj = std::make_shared<ExceptionThrowingObject>("ThrowObj");
  sen::impl::getWorkQueue(obj.get())->enable();

  bool nonConstCaught = false;
  auto cb1 = sen::MethodCallback<void>(sen::impl::getWorkQueue(obj.get()),
                                       [&](const auto&, const auto& res) { nonConstCaught = res.isError(); });

  obj->testAsyncCall(std::move(cb1));

  bool constCaught = false;
  auto cb2 = sen::MethodCallback<void>(sen::impl::getWorkQueue(obj.get()),
                                       [&](const auto&, const auto& res) { constCaught = res.isError(); });

  obj->testAsyncCallConst(std::move(cb2));

  while (sen::impl::getWorkQueue(obj.get())->executeAll())
  {
  }

  EXPECT_TRUE(nonConstCaught);
  EXPECT_TRUE(constCaught);
}

/// @test
/// Confirms exceptions thrown from natively executed non-void methods are safely caught and bubbled up as errors
/// @requirements(SEN-351)
TEST(NativeObject, ExecuteCallWithArgsNonVoidException)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();
  const auto obj = std::make_shared<ExceptionThrowingObject>("ThrowObj");
  sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);

  bool errorHit = false;
  auto cb = sen::MethodCallback<int>(&queue, [&](const auto&, const auto& res) { errorHit = res.isError(); });

  obj->testAsyncCallNonVoid(std::move(cb));

  while (queue.executeAll())
  {
  }

  EXPECT_TRUE(errorHit);
}

/// @test
/// Verifies deferred async calls appropriately handle immediate exceptions, void resolutions, and future polling
/// @requirements(SEN-351)
TEST(NativeObject, DeferredCalls)
{
  const auto obj = std::make_shared<DeferredObject>("DefObj");
  sen::impl::getWorkQueue(obj.get())->enable();

  bool voidSuccess = false;
  auto cbVoid = sen::MethodCallback<void>(sen::impl::getWorkQueue(obj.get()),
                                          [&](const auto&, const auto& res) { voidSuccess = res.isOk(); });
  obj->testDeferredCallVoid(std::move(cbVoid));

  int resultValue = 0;
  auto cbVal = sen::MethodCallback<int>(sen::impl::getWorkQueue(obj.get()),
                                        [&](const auto&, const auto& res)
                                        {
                                          if (res.isOk())
                                          {
                                            resultValue = res.getValue();
                                          }
                                        });
  obj->testDeferredCall(std::move(cbVal));

  bool exceptionCaught = false;
  auto cbEx = sen::MethodCallback<int>(sen::impl::getWorkQueue(obj.get()),
                                       [&](const auto&, const auto& res) { exceptionCaught = res.isError(); });
  obj->testDeferredCallException(std::move(cbEx));

  while (sen::impl::getWorkQueue(obj.get())->executeAll())
  {
  }

  EXPECT_TRUE(voidSuccess);
  EXPECT_EQ(resultValue, 42);
  EXPECT_TRUE(exceptionCaught);
}

/// @test
/// Verifies variant calls accurately catch parameter exceptions (e.g., out of range or bad conversion)
/// @requirements(SEN-351)
TEST(NativeObject, VariantCallCatchesExceptions)
{
  const auto obj = std::make_shared<PublicNativeObject>("ThrowObj");
  sen::impl::getWorkQueue(obj.get())->enable();

  const auto* method = obj->getClass()->searchMethodByName("setNextProp2");

  bool catchAllHit = false;
  auto cb1 = sen::MethodCallback<sen::Var>(sen::impl::getWorkQueue(obj.get()),
                                           [&](const auto&, const auto& res) { catchAllHit = res.isError(); });

  obj->invokeUntyped(method, {}, std::move(cb1));

  bool runtimeErrorHit = false;
  auto cb2 = sen::MethodCallback<sen::Var>(sen::impl::getWorkQueue(obj.get()),
                                           [&](const auto&, const auto& res) { runtimeErrorHit = res.isError(); });

  const sen::KeyedVar keyed(1, std::make_shared<sen::Var>(2));
  obj->invokeUntyped(method, {sen::Var(keyed)}, std::move(cb2));

  while (sen::impl::getWorkQueue(obj.get())->executeAll())
  {
  }

  EXPECT_TRUE(catchAllHit);
  EXPECT_TRUE(runtimeErrorHit);
}

/// @test
/// Validates that variant calls gracefully catch std::runtime_error and forward it to the callback
/// @requirements(SEN-351)
TEST(NativeObject, VariantCallCatchesRuntimeError)
{
  sen::impl::WorkQueue queue(50, false);
  queue.enable();
  const auto obj = std::make_shared<VariantThrowingObject>("ThrowObj");
  sen::kernel::impl::Runner::setQueues(obj.get(), &queue, nullptr);
  const auto* method = obj->getClass()->searchMethodByName("setNextProp1");

  bool caught = false;
  auto cb = sen::MethodCallback<sen::Var>(&queue, [&](const auto&, const auto& res) { caught = res.isError(); });

  obj->invokeUntyped(method, {}, std::move(cb));

  while (queue.executeAll())
  {
  }

  EXPECT_TRUE(caught);
}

/// @test
/// Evaluates robust VarMap parsing with success, fallback defaults, and validation bounds
/// @requirements(SEN-351)
TEST(NativeObjectImpl, MapHelpers)
{
  sen::VarMap map;
  map["test_key"] = sen::Var(42U);

  uint32_t val1 = 0U;
  bool validCalled1 = false;
  sen::impl::tryGetFromMap<uint32_t>("test_key", map, val1, [&](const uint32_t&) { validCalled1 = true; });
  EXPECT_EQ(val1, 42U);
  EXPECT_TRUE(validCalled1);

  uint32_t valNoVal = 0U;
  sen::impl::tryGetFromMap<uint32_t>("test_key", map, valNoVal);
  EXPECT_EQ(valNoVal, 42U);

  uint32_t val2 = 99U;
  bool validCalled2 = false;
  sen::impl::tryGetFromMap<uint32_t>("missing_key", map, val2, [&](const uint32_t&) { validCalled2 = true; });
  EXPECT_EQ(val2, 99U);
  EXPECT_FALSE(validCalled2);

  bool validCalled3 = false;
  const auto val3 = sen::impl::getFromMap<uint32_t>("test_key", map, [&](const uint32_t&) { validCalled3 = true; });
  EXPECT_EQ(val3, 42U);
  EXPECT_TRUE(validCalled3);

  const auto val4 = sen::impl::getFromMap<uint32_t>("test_key", map);
  EXPECT_EQ(val4, 42U);

  EXPECT_ANY_THROW(sen::impl::getFromMap<uint32_t>("missing_key", map));
}

/// @test
/// Validates proper handling of constructor parameters using VarMap and tryGetFromMap/getFromMap templates
/// @requirements(SEN-351)
TEST(NativeObject, ConstructorWithVarMap)
{
  sen::VarMap map;
  map["prop1"] = sen::Var(std::string("hello_var"));
  map["prop2"] = sen::Var(42U);
  const example_class::ExampleClassBase obj("test_varMap", map);

  EXPECT_EQ(obj.getProp1(), "hello_var");
  EXPECT_EQ(obj.getProp2(), 42U);

  bool validCalled = false;
  const auto val = sen::impl::getFromMap<uint32_t>("prop2", map, [&](const uint32_t& v) { validCalled = v == 42U; });

  EXPECT_EQ(val, 42U);
  EXPECT_TRUE(validCalled);
}

/// @test
/// Validates inline template wrappers for converting Variants to OutputStreams and back
/// @requirements(SEN-351)
TEST(NativeObjectImpl, VariantStreamHelpers)
{
  std::vector<uint8_t> buffer;
  sen::ResizableBufferWriter writer(buffer);
  sen::OutputStream out(writer);

  const sen::Var vStr(std::string("test_stream"));
  sen::impl::variantToStream<std::string>(out, vStr);

  const sen::Var vTs(sen::TimeStamp {12345});
  sen::impl::variantToStream<sen::TimeStamp>(out, vTs);

  const sen::Var vDur(sen::Duration {67890});
  sen::impl::variantToStream<sen::Duration>(out, vDur);

  sen::InputStream in(sen::makeConstSpan(buffer));
  const sen::Var resultStr = sen::impl::variantFromStream<std::string>(in);
  EXPECT_EQ(resultStr.get<std::string>(), "test_stream");

  const sen::Var resultTs = sen::impl::variantFromStream<sen::TimeStamp>(in);
  EXPECT_EQ(resultTs.get<sen::TimeStamp>().sinceEpoch().getNanoseconds(), 12345);

  const sen::Var resultDur = sen::impl::variantFromStream<sen::Duration>(in);
  EXPECT_EQ(resultDur.get<sen::Duration>().getNanoseconds(), 67890);
}

/// @test
/// Validates inline template wrappers for explicitly invoking stream deserialization without variant overhead
/// @requirements(SEN-351)
TEST(NativeObjectImpl, RawStreamHelpers)
{
  std::vector<uint8_t> buffer;
  sen::ResizableBufferWriter writer(buffer);
  sen::OutputStream out(writer);

  out.writeUInt32(777U);

  sen::InputStream in(sen::makeConstSpan(buffer));
  const auto result = sen::impl::fromStream<uint32_t>(in);

  EXPECT_EQ(result, 777U);
}

/// @test
/// Ensures writePropertyIfChanged correctly skips unchanged properties and evaluates reliably
/// @requirements(SEN-351)
TEST(NativeObjectImpl, WritePropertyIfChangedStreamHelper)
{
  std::vector<uint8_t> buffer;
  sen::ResizableBufferWriter writer(buffer);
  sen::OutputStream out(writer);

  sen::impl::PropertyFlags flags;
  flags.setTypesInSync(true);

  sen::impl::writePropertyIfChanged<uint32_t>(flags, 0x1234U, out, 42U);
  EXPECT_TRUE(buffer.empty());

  std::ignore = flags.advanceNext();
  flags.advanceCurrent();

  sen::impl::writePropertyIfChanged<uint32_t>(flags, 0x1234U, out, 42U);
  EXPECT_FALSE(buffer.empty());
}

/// @test
/// Verifies that BufferProviders are correctly invoked when changed properties are written to streams
/// @requirements(SEN-351)
TEST(NativeObjectImpl, WriteChangedPropertiesToStreamBufferProvider)
{
  sen::impl::PropertyFlags flags;
  flags.setTypesInSync(true);

  const sen::impl::BufferProvider throwingProvider =
    [](std::size_t) -> sen::ResizableBufferWriter<sen::FixedMemoryBlock>
  { throw std::runtime_error("Provider called"); };

  EXPECT_NO_THROW(sen::impl::writePropertyIfChanged<uint32_t>(flags, 0x1234U, throwingProvider, 42U));

  std::ignore = flags.advanceNext();
  flags.advanceCurrent();

  EXPECT_THROW(sen::impl::writePropertyIfChanged<uint32_t>(flags, 0x1234U, throwingProvider, 42U), std::runtime_error);
}

/// @test
/// Ensures that creating an object with an empty name fails securely and throws a runtime error
/// @requirements(SEN-351)
TEST(NativeObject, EmptyNameThrowsError)
{
  EXPECT_ANY_THROW({ PublicNativeObject obj(""); });
}

/// @test
/// Ensures that creating an object with a name containing spaces fails securely and throws a runtime error
/// @requirements(SEN-351)
TEST(NativeObject, NameWithSpacesThrowsError)
{
  EXPECT_ANY_THROW({ PublicNativeObject obj("Invalid Name"); });
}

/// @test
/// Verifies the internal helper accurately delegates streaming to the polymorphic instance
/// @requirements(SEN-351)
TEST(NativeObject, WriteAllPropertiesToStreamDelegation)
{
  PublicNativeObject obj("StreamObj");
  obj.setNextProp1("test_value");
  obj.commit(sen::TimeStamp {100});

  std::vector<uint8_t> buffer;
  sen::ResizableBufferWriter writer(buffer);
  sen::OutputStream out(writer);

  sen::impl::writeAllPropertiesToStream(&obj, out);

  EXPECT_FALSE(buffer.empty());
}
