// === remote_object_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/memory_block.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/detail/work_queue.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using sen::EventCallback;
using sen::MemberHash;
using sen::MethodCallback;
using sen::MethodResult;
using sen::TransportMode;
using sen::Var;
using sen::VarList;
using sen::impl::CallId;
using sen::impl::MethodResponseData;
using sen::impl::RemoteCallResult;
using sen::impl::RemoteObjectInfo;
using sen::impl::WorkQueue;

namespace
{

class TestRemoteObject final: public example_class::ExampleClassRemoteProxy
{
public:
  using ExampleClassRemoteProxy::ExampleClassRemoteProxy;
  using RemoteObject::adaptAndMakeRemoteCall;
  using RemoteObject::checkMemberTypesInSyncInDetail;
  using RemoteObject::dispatchEventFromStream;
  using RemoteObject::invalidateTransport;
  using RemoteObject::makeRemoteCall;
  using RemoteObject::memberTypesInSync;
  using RemoteObject::propertyUpdatesReceived;
  using RemoteObject::readOrAdapt;
  using RemoteObject::responseReceived;
  using RemoteObject::sendCall;
  using RemoteObject::senImplEventEmitted;
  using RemoteObject::senImplRemoveUntypedConnection;
};

RemoteObjectInfo createTestInfo(WorkQueue* queue, sen::impl::SendCallFunc sendFunc = nullptr)
{
  return RemoteObjectInfo {example_class::ExampleClassInterface::meta(),
                           "TestObj",
                           sen::ObjectId {77U},
                           queue,
                           sendFunc ? std::move(sendFunc) : [](auto...) -> sen::Result<CallId, std::string>
                           { return sen::Ok(CallId {1U}); },
                           "localTestObj",
                           nullptr,
                           sen::ObjectOwnerId {1U},
                           {},
                           std::nullopt};
}

}  // namespace

/// @test
/// Reports itself as remote with the identity given at construction, casts to both proxy and
/// remote object, and starts with no participant and no writer schema.
/// @requirements(SEN-351)
TEST(RemoteObject, IdentityAndPolymorphicCasts)
{
  WorkQueue queue(50, false);
  const auto obj = std::make_shared<TestRemoteObject>(createTestInfo(&queue));

  EXPECT_TRUE(obj->isRemote());
  EXPECT_EQ(obj->getId().get(), 77U);
  EXPECT_EQ(obj->getName(), "TestObj");
  EXPECT_EQ(obj->getLocalName(), "localTestObj");
  EXPECT_EQ(obj->getClass()->getName(), "ExampleClass");

  const TestRemoteObject* constObj = obj.get();
  EXPECT_EQ(constObj->asProxyObject(), obj.get());
  EXPECT_EQ(constObj->asRemoteObject(), obj.get());
  EXPECT_EQ(obj->asProxyObject(), obj.get());
  EXPECT_EQ(obj->asRemoteObject(), obj.get());

  EXPECT_TRUE(obj->getParticipant().expired());
  EXPECT_FALSE(obj->getWriterSchema().has_value());
}

/// @test
/// Copies the last commit time across through copyStateFrom and invokes the destruction callback
/// when the proxy is destroyed.
/// @requirements(SEN-351)
TEST(RemoteObject, StateMigrationAndDestruction)
{
  WorkQueue queue(50, false);
  bool destructionTriggered = false;

  auto info = createTestInfo(&queue);
  info.destructionCallback = [&](sen::impl::RemoteObject*) { destructionTriggered = true; };

  {
    const auto obj = std::make_shared<TestRemoteObject>(std::move(info));
    const auto obj2 = std::make_shared<TestRemoteObject>(createTestInfo(&queue));

    obj2->initializeState({}, sen::TimeStamp {404});

    static_cast<sen::impl::RemoteObject&>(*obj).copyStateFrom(*obj2);
    EXPECT_EQ(obj->getLastCommitTime().sinceEpoch().getNanoseconds(), 404);
  }

  EXPECT_TRUE(destructionTriggered);
}

/// @test
/// Routes emitted property-change and event payloads to untyped subscriptions, stops delivering
/// once the connection guard is destroyed, and tolerates removing an unknown connection.
/// @requirements(SEN-351, SEN-574)
TEST(RemoteObject, UntypedEventSubscriptionAndRouting)
{
  WorkQueue queue(50, false);
  queue.enable();

  const auto obj = std::make_shared<TestRemoteObject>(createTestInfo(&queue));
  const auto* meta = example_class::ExampleClassInterface::meta().type();
  const auto* prop1 = meta->searchPropertyByName("prop1");
  const auto* ev = meta->searchEventByName("prop1Changed");

  obj->senImplRemoveUntypedConnection(sen::ConnId {1}, MemberHash {123U});

  bool propertyTriggered = false;
  {
    auto cb = EventCallback<VarList>(&queue, [&](const sen::EventInfo&, VarList) { propertyTriggered = true; });
    auto guard = obj->onPropertyChangedUntyped(prop1, std::move(cb));

    obj->senImplEventEmitted(prop1->getId(), [] { return VarList {}; }, sen::EventInfo {sen::TimeStamp {0}});
    std::ignore = queue.executeAll();
    EXPECT_TRUE(propertyTriggered);
    propertyTriggered = false;
  }

  obj->senImplEventEmitted(prop1->getId(), [] { return VarList {}; }, sen::EventInfo {sen::TimeStamp {0}});
  std::ignore = queue.executeAll();
  EXPECT_FALSE(propertyTriggered);

  bool eventTriggered = false;
  auto evCb = EventCallback<VarList>(&queue, [&](const sen::EventInfo&, VarList) { eventTriggered = true; });
  auto evGuard = obj->onEventUntyped(ev, std::move(evCb));

  obj->senImplEventEmitted(ev->getId(), [] { return VarList {}; }, sen::EventInfo {sen::TimeStamp {0}});
  std::ignore = queue.executeAll();
  EXPECT_TRUE(eventTriggered);
}

/// @test
/// Survives subscribing to untyped events and property changes with default-constructed, invalid
/// callbacks.
/// @requirements(SEN-351)
TEST(RemoteObject, IgnoresInvalidatedCallbacks)
{
  WorkQueue queue(50, false);
  const auto obj = std::make_shared<TestRemoteObject>(createTestInfo(&queue));
  const auto* meta = example_class::ExampleClassInterface::meta().type();
  const auto* prop = meta->searchPropertyByName("prop1");
  const auto* ev = meta->searchEventByName("prop1Changed");

  EventCallback<VarList> emptyCb1;
  EventCallback<VarList> emptyCb2;

  auto guard1 = obj->onEventUntyped(ev, std::move(emptyCb1));
  auto guard2 = obj->onPropertyChangedUntyped(prop, std::move(emptyCb2));
}

/// @test
/// Deserializes a successful non-void method response and delivers the decoded Duration value to
/// the pending callback.
/// @requirements(SEN-351, SEN-1051)
TEST(RemoteObject, ProcessSuccessfulNonVoidMethodResponses)
{
  WorkQueue queue(50, false);
  queue.enable();

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {42U}); });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  bool callbackInvoked = false;
  auto cb = MethodCallback<sen::Duration>(&queue,
                                          [&](const sen::MethodCallInfo&, const MethodResult<sen::Duration>& res)
                                          {
                                            callbackInvoked = true;
                                            EXPECT_TRUE(res.isOk());
                                            EXPECT_EQ(res.getValue().getNanoseconds(), 999);
                                          });

  obj->makeRemoteCall<sen::Duration>(MemberHash {123U}, TransportMode::confirmed, std::move(cb));

  MethodResponseData resp;
  resp.callId = 42U;
  resp.result = RemoteCallResult::success;

  const auto buf = std::make_shared<std::vector<uint8_t>>();
  sen::ResizableBufferWriter writer(*buf);
  sen::OutputStream out(writer);
  sen::SerializationTraits<sen::Duration>::write(out, sen::Duration(999));

  resp.returnValBuffer = buf;

  EXPECT_TRUE(obj->responseReceived(resp));
  std::ignore = queue.executeAll();
  EXPECT_TRUE(callbackInvoked);
}

/// @test
/// Turns each remote error code, logic error, object not found, runtime error, and unknown
/// exception, into an error result on the pending callback.
/// @requirements(SEN-351, SEN-1049)
TEST(RemoteObject, ProcessErrorMethodResponses)
{
  WorkQueue queue(50, false);
  queue.enable();

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {1U}); });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  auto executeErrorTest = [&](const RemoteCallResult code)
  {
    bool invoked = false;
    auto cb = MethodCallback<void>(&queue,
                                   [&](const sen::MethodCallInfo&, const MethodResult<void>& res)
                                   {
                                     invoked = true;
                                     EXPECT_TRUE(res.isError());
                                   });

    obj->makeRemoteCall<void>(MemberHash {123U}, TransportMode::confirmed, std::move(cb));

    MethodResponseData resp;
    resp.callId = 1U;
    resp.result = code;
    resp.error = "test error message injection";
    resp.returnValBuffer = std::make_shared<std::vector<uint8_t>>();

    std::ignore = obj->responseReceived(resp);
    std::ignore = queue.executeAll();

    EXPECT_TRUE(invoked);
  };

  executeErrorTest(RemoteCallResult::logicError);
  executeErrorTest(RemoteCallResult::objectNotFound);
  executeErrorTest(RemoteCallResult::runtimeError);
  executeErrorTest(RemoteCallResult::unknownException);
}

/// @test
/// Completes a void remote call with an ok result when the success response arrives.
/// @requirements(SEN-351)
TEST(RemoteObject, MakeRemoteCall_VoidMethodSuccess)
{
  WorkQueue queue(50, false);
  queue.enable();

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {99U}); });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  bool successHit = false;
  auto cb = MethodCallback<void>(&queue,
                                 [&](const auto&, const auto& res)
                                 {
                                   successHit = true;
                                   EXPECT_TRUE(res.isOk());
                                 });

  obj->makeRemoteCall<void>(MemberHash {123U}, TransportMode::confirmed, std::move(cb));

  MethodResponseData resp;
  resp.callId = 99U;
  resp.result = RemoteCallResult::success;
  resp.returnValBuffer = std::make_shared<std::vector<uint8_t>>();

  EXPECT_TRUE(obj->responseReceived(resp));
  std::ignore = queue.executeAll();
  EXPECT_TRUE(successHit);
}

/// @test
/// Serializes a string and a uint32 argument into the outgoing call buffer, the transport-side
/// reader decodes the same values back.
/// @requirements(SEN-351)
TEST(RemoteObject, MakeRemoteCall_WithArguments_SerializesCorrectly)
{
  WorkQueue queue(50, false);
  queue.enable();

  std::string capturedStr;
  uint32_t capturedU32 = 0;

  auto info = createTestInfo(&queue,
                             [&](TransportMode, sen::ObjectOwnerId, sen::ObjectId, MemberHash, sen::MemBlockPtr&& args)
                               -> sen::Result<CallId, std::string>
                             {
                               sen::InputStream in(sen::makeConstSpan(args->data(), args->size()));
                               sen::SerializationTraits<std::string>::read(in, capturedStr);
                               sen::SerializationTraits<uint32_t>::read(in, capturedU32);
                               return sen::Ok(CallId {1U});
                             });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  auto cb = MethodCallback<void>(&queue, [](const auto&, const auto&) {});

  obj->makeRemoteCall<void, std::string, uint32_t>(
    MemberHash {1}, TransportMode::confirmed, std::move(cb), "hello_world", 42U);

  EXPECT_EQ(capturedStr, "hello_world");
  EXPECT_EQ(capturedU32, 42U);
}

/// @test
/// Reports an error to the callback right away when the transport send function fails.
/// @requirements(SEN-351)
TEST(RemoteObject, MakeRemoteCall_SendCallFails_ReturnsErrorImmediately)
{
  WorkQueue queue(50, false);
  queue.enable();

  auto info = createTestInfo(
    &queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Err(std::string("transport offline")); });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  bool errorHit = false;
  auto cb = MethodCallback<void>(&queue,
                                 [&](const auto&, const auto& res)
                                 {
                                   errorHit = true;
                                   EXPECT_TRUE(res.isError());
                                 });

  obj->makeRemoteCall<void>(MemberHash {123U}, TransportMode::confirmed, std::move(cb));

  std::ignore = queue.executeAll();
  EXPECT_TRUE(errorHit);
}

/// @test
/// Dismisses the response of a call made with an empty callback, responseReceived reports it as
/// unconsumed.
/// @requirements(SEN-351)
TEST(RemoteObject, MakeRemoteCall_DismissesResponseIfNoCallback)
{
  WorkQueue queue(50, false);
  queue.enable();

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {42U}); });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  MethodCallback<void> emptyCb;
  obj->makeRemoteCall<void>(MemberHash {123U}, TransportMode::confirmed, std::move(emptyCb));

  MethodResponseData resp;
  resp.callId = 42U;
  resp.result = RemoteCallResult::success;
  resp.returnValBuffer = std::make_shared<std::vector<uint8_t>>();

  EXPECT_FALSE(obj->responseReceived(resp));
}

/// @test
/// Cancels the pending call when the transport is invalidated, the callback receives an error
/// instead of waiting forever.
/// @requirements(SEN-351)
TEST(RemoteObject, CancelsPendingCallsWhenTransportInvalidated)
{
  WorkQueue queue(50, false);
  queue.enable();

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {42U}); });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  bool errorHit = false;
  auto cb = MethodCallback<void>(&queue,
                                 [&](const sen::MethodCallInfo&, const MethodResult<void>& res)
                                 {
                                   errorHit = true;
                                   EXPECT_TRUE(res.isError());
                                 });

  obj->makeRemoteCall<void>(MemberHash {123U}, TransportMode::confirmed, std::move(cb));
  obj->invalidateTransport();

  std::ignore = queue.executeAll();
  EXPECT_TRUE(errorHit);
}

/// @test
/// Returns an error from sendCall immediately when the transport has already been invalidated.
/// @requirements(SEN-351)
TEST(RemoteObject, FastFailsSendCallOnInvalidTransport)
{
  WorkQueue queue(50, false);
  const auto obj = std::make_shared<TestRemoteObject>(createTestInfo(&queue));

  obj->invalidateTransport();

  const auto result =
    obj->sendCall(TransportMode::confirmed, sen::ObjectOwnerId {1U}, sen::ObjectId {1U}, MemberHash {1U}, nullptr);
  EXPECT_TRUE(result.isError());
}

/// @test
/// Carries an untyped method invocation through the remote-call path and delivers an ok variant
/// result when the success response arrives.
/// @requirements(SEN-351)
TEST(RemoteObject, InvokeUntyped_SerializesArgsAndProcessesSuccess)
{
  WorkQueue queue(50, false);
  queue.enable();

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {99U}); });
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  const auto* meta = example_class::ExampleClassInterface::meta().type();
  const auto* method = meta->searchMethodByName("setNextProp1");

  bool invoked = false;
  auto cb = MethodCallback<Var>(&queue,
                                [&](const sen::MethodCallInfo&, const MethodResult<Var>& res)
                                {
                                  invoked = true;
                                  EXPECT_TRUE(res.isOk());
                                });

  const VarList args = {Var(std::string("dynamic_test_string"))};
  obj->invokeUntyped(method, args, std::move(cb));

  MethodResponseData resp;
  resp.callId = 99U;
  resp.result = RemoteCallResult::success;
  resp.returnValBuffer = std::make_shared<std::vector<uint8_t>>();

  std::ignore = obj->responseReceived(resp);
  std::ignore = queue.executeAll();

  EXPECT_TRUE(invoked);
}

/// @test
/// Reports an error to the callback immediately when invokeUntyped runs against an invalidated
/// transport.
/// @requirements(SEN-351)
TEST(RemoteObject, InvokeUntyped_FailsGracefullyOnInvalidTransport)
{
  WorkQueue queue(50, false);
  queue.enable();

  const auto obj = std::make_shared<TestRemoteObject>(createTestInfo(&queue));
  const auto* method = example_class::ExampleClassInterface::meta().type()->searchMethodByName("setNextProp1");

  obj->invalidateTransport();

  bool errorHit = false;
  auto cb = MethodCallback<Var>(&queue,
                                [&](const sen::MethodCallInfo&, const MethodResult<Var>& res)
                                {
                                  errorHit = true;
                                  EXPECT_TRUE(res.isError());
                                });

  obj->invokeUntyped(method, {Var(std::string("test"))}, std::move(cb));

  std::ignore = queue.executeAll();
  EXPECT_TRUE(errorHit);
}

/// @test
/// Adapts a property value written under a writer schema with a different type, a uint64 payload
/// lands in the local uint32 prop2 during initializeState.
/// @requirements(SEN-351, SEN-573)
TEST(RemoteObject, InitializesStateWithWriterSchemaAdaptation)
{
  WorkQueue queue(50, false);

  sen::ClassSpec spec;
  spec.name = "CustomSchemaProp";
  spec.qualifiedName = "pkg.CustomSchemaProp";
  spec.properties.emplace_back(
    "prop2", "", sen::UInt64Type::get(), sen::PropertyCategory::dynamicRW, TransportMode::multicast);
  auto customSchema = sen::ClassType::make(spec);

  auto info = createTestInfo(&queue);
  info.writerSchema = customSchema;
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  std::vector<uint8_t> payload;
  sen::ResizableBufferWriter writer(payload);
  sen::OutputStream out(writer);

  const auto* writerProp2 = customSchema->searchPropertyByName("prop2");
  out.writeUInt32(writerProp2->getId().get());
  out.writeUInt32(8U);
  out.writeUInt64(123456ULL);

  obj->initializeState(sen::makeConstSpan(payload), sen::TimeStamp {100});
  EXPECT_EQ(obj->getProp2(), 123456U);
}

/// @test
/// Fails an adapted call with an error when the writer schema's method expects more arguments
/// than the local caller supplies.
/// @requirements(SEN-351, SEN-573)
TEST(RemoteObject, AdaptRemoteCall_FailsOnArgumentMismatch)
{
  WorkQueue queue(50, false);
  queue.enable();

  sen::CallableSpec cSpec;
  cSpec.name = "testMethod";
  cSpec.args.push_back(sen::Arg {"arg1", {}, sen::StringType::get()});
  cSpec.transportMode = TransportMode::confirmed;
  sen::MethodSpec readerSpec(cSpec, sen::VoidType::get(), sen::Constness::nonConstant, sen::NonPropertyRelated {});
  auto localSchema = sen::ClassType::make(
    sen::ClassSpec {"Local", "pkg.Local", "", {}, {readerSpec}, {}, {}, {}, false, false, nullptr, nullptr});
  const auto* readerMethod = localSchema->searchMethodByName("testMethod");

  sen::CallableSpec cSpecWriter;
  cSpecWriter.name = "testMethod";
  cSpecWriter.args.push_back(sen::Arg {"arg1", {}, sen::StringType::get()});
  cSpecWriter.args.push_back(sen::Arg {"arg2", {}, sen::UInt32Type::get()});
  cSpecWriter.transportMode = TransportMode::confirmed;
  sen::MethodSpec writerSpec(
    cSpecWriter, sen::VoidType::get(), sen::Constness::nonConstant, sen::NonPropertyRelated {});
  auto writerSchema = sen::ClassType::make(
    sen::ClassSpec {"Writer", "pkg.Writer", "", {}, {writerSpec}, {}, {}, {}, false, false, nullptr, nullptr});

  auto info = createTestInfo(&queue);
  info.writerSchema = writerSchema;
  auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  bool errorHit = false;
  auto cb = MethodCallback<void>(&queue,
                                 [&](const auto&, const auto& res)
                                 {
                                   errorHit = true;
                                   EXPECT_TRUE(res.isError());
                                 });

  obj->adaptAndMakeRemoteCall<void>(
    readerMethod->getId(), readerMethod, TransportMode::confirmed, std::move(cb), std::string("test"));

  std::ignore = queue.executeAll();
  EXPECT_TRUE(errorHit);
}

/// @test
/// Rejects an adapted call with an error when the writer schema lacks the target method.
/// @requirements(SEN-351)
TEST(RemoteObject, FailsCallingMissingMethodInWriterSchema)
{
  WorkQueue queue(50, false);
  queue.enable();

  sen::ClassSpec spec;
  spec.name = "EmptySchema";
  spec.qualifiedName = "pkg.EmptySchema";
  auto customSchema = sen::ClassType::make(spec);

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {42U}); });
  info.writerSchema = customSchema;
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  const auto* meta = example_class::ExampleClassInterface::meta().type();
  const auto* readerMethod = meta->searchMethodByName("setNextProp1");

  bool errorHit = false;
  auto cb = MethodCallback<void>(&queue,
                                 [&](const auto&, const auto& res)
                                 {
                                   errorHit = true;
                                   EXPECT_TRUE(res.isError());
                                 });

  obj->adaptAndMakeRemoteCall<void>(
    readerMethod->getId(), readerMethod, TransportMode::confirmed, std::move(cb), std::string("test"));

  std::ignore = queue.executeAll();
  EXPECT_TRUE(errorHit);
}

/// @test
/// Completes an adapted call with an ok result when the writer schema's method matches the local
/// signature.
/// @requirements(SEN-351, SEN-573)
TEST(RemoteObject, AdaptsMethodSuccessfully)
{
  WorkQueue queue(50, false);
  queue.enable();

  sen::CallableSpec cSpec;
  cSpec.name = "setNextProp1";
  cSpec.args.push_back(sen::Arg {"prop1", {}, sen::StringType::get()});
  cSpec.transportMode = TransportMode::confirmed;

  const sen::MethodSpec mSpecMatch(
    cSpec, sen::VoidType::get(), sen::Constness::nonConstant, sen::NonPropertyRelated {});

  sen::ClassSpec spec;
  spec.name = "CustomSchemaMatch";
  spec.qualifiedName = "pkg.CustomSchemaMatch";
  spec.methods.push_back(mSpecMatch);
  auto customSchema = sen::ClassType::make(spec);

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {42U}); });
  info.writerSchema = customSchema;
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  const auto* meta = example_class::ExampleClassInterface::meta().type();
  const auto* readerMethod = meta->searchMethodByName("setNextProp1");

  bool hit = false;
  auto cb = MethodCallback<void>(&queue,
                                 [&](const auto&, const auto& res)
                                 {
                                   hit = true;
                                   EXPECT_TRUE(res.isOk());
                                 });

  obj->adaptAndMakeRemoteCall<void>(
    readerMethod->getId(), readerMethod, TransportMode::confirmed, std::move(cb), std::string("test"));

  MethodResponseData resp;
  resp.callId = 42U;
  resp.result = RemoteCallResult::success;
  resp.returnValBuffer = std::make_shared<std::vector<uint8_t>>();

  std::ignore = obj->responseReceived(resp);
  std::ignore = queue.executeAll();

  EXPECT_TRUE(hit);
}

/// @test
/// Matches adapted-call arguments to the writer schema's parameter list and completes with an ok
/// result once the response arrives.
/// @requirements(SEN-351)
TEST(RemoteObject, AdaptAndMakeRemoteCall_SerializesArgumentsProperly)
{
  WorkQueue queue(50, false);
  queue.enable();

  sen::CallableSpec cSpec;
  cSpec.name = "setNextProp1";
  cSpec.args.push_back(sen::Arg {"prop1", {}, sen::StringType::get()});
  cSpec.transportMode = TransportMode::confirmed;
  const sen::MethodSpec mSpecMatch(
    cSpec, sen::VoidType::get(), sen::Constness::nonConstant, sen::NonPropertyRelated {});

  sen::ClassSpec spec;
  spec.name = "CustomSchemaArgs";
  spec.qualifiedName = "pkg.CustomSchemaArgs";
  spec.methods.push_back(mSpecMatch);
  auto customSchema = sen::ClassType::make(spec);

  auto info = createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {42U}); });
  info.writerSchema = customSchema;
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  const auto* meta = example_class::ExampleClassInterface::meta().type();
  const auto* readerMethod = meta->searchMethodByName("setNextProp1");
  const auto* writerMethod = customSchema->searchMethodByName("setNextProp1");

  bool successHit = false;
  auto cb = MethodCallback<void>(&queue,
                                 [&](const auto&, const auto& res)
                                 {
                                   successHit = true;
                                   EXPECT_TRUE(res.isOk());
                                 });

  obj->adaptAndMakeRemoteCall<void>(
    writerMethod->getId(), readerMethod, TransportMode::confirmed, std::move(cb), std::string("adapted_test"));

  MethodResponseData resp;
  resp.callId = 42U;
  resp.result = RemoteCallResult::success;
  resp.returnValBuffer = std::make_shared<std::vector<uint8_t>>();

  std::ignore = obj->responseReceived(resp);
  std::ignore = queue.executeAll();

  EXPECT_TRUE(successHit);
}

/// @test
/// Delivers only the locally expected leading argument when a writer event carries an extra
/// trailing argument, the surplus payload value is dropped during stream dispatch.
/// @requirements(SEN-351, SEN-573)
TEST(RemoteObject, FiltersExtendedEventArgsDuringStreamDispatch)
{
  WorkQueue queue(50, false);
  queue.enable();

  sen::CallableSpec evSpec;
  evSpec.name = "testEvent";
  evSpec.args.push_back(sen::Arg {"prop1", {}, sen::StringType::get()});
  evSpec.args.push_back(sen::Arg {"extra", {}, sen::UInt32Type::get()});
  evSpec.transportMode = TransportMode::multicast;

  sen::ClassSpec spec;
  spec.name = "CustomSchemaEvent";
  spec.qualifiedName = "pkg.CustomSchemaEvent";
  spec.events.push_back(sen::EventSpec {evSpec});
  auto customSchema = sen::ClassType::make(spec);

  auto info = createTestInfo(&queue);
  info.writerSchema = customSchema;
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  std::vector<uint8_t> payload;
  sen::ResizableBufferWriter writer(payload);
  sen::OutputStream out(writer);
  sen::SerializationTraits<std::string>::write(out, "hello");
  sen::SerializationTraits<uint32_t>::write(out, 99U);

  sen::impl::EventBuffer<std::string> localBuffer;
  bool eventFired = false;
  std::string receivedVal;

  auto cb = EventCallback<std::string>(&queue,
                                       [&](const sen::EventInfo&, const std::string& val)
                                       {
                                         eventFired = true;
                                         receivedVal = val;
                                       });
  auto guard = localBuffer.addConnection(obj.get(), std::move(cb), sen::ConnId {1});

  const auto* writerEvent = customSchema->searchEventByName("testEvent");

  std::vector readerArgs = {sen::Arg {"prop1", {}, sen::StringType::get()}};

  obj->dispatchEventFromStream(localBuffer,
                               false,
                               readerArgs,
                               writerEvent->getId(),
                               sen::TimeStamp {0},
                               sen::ObjectId {1},
                               TransportMode::multicast,
                               sen::makeConstSpan(payload),
                               obj.get());

  std::ignore = queue.executeAll();

  EXPECT_TRUE(eventFired);
  EXPECT_EQ(receivedVal, "hello");
}

/// @test
/// Skips an unknown property id and its payload during initializeState and still decodes the
/// following known property.
/// @requirements(SEN-351)
TEST(RemoteObject, InitializeState_SafelySkipsUnknownProperties)
{
  WorkQueue queue(50, false);
  const auto obj = std::make_shared<TestRemoteObject>(createTestInfo(&queue));

  std::vector<uint8_t> payload;
  sen::ResizableBufferWriter writer(payload);
  sen::OutputStream out(writer);

  out.writeUInt32(0x99999999U);
  out.writeUInt32(4U);
  out.writeUInt32(0xFFFFFFFFU);

  const auto* prop2 = example_class::ExampleClassInterface::meta().type()->searchPropertyByName("prop2");
  out.writeUInt32(prop2->getId().get());
  out.writeUInt32(4U);
  out.writeUInt32(42U);

  obj->initializeState(sen::makeConstSpan(payload), sen::TimeStamp {100});

  EXPECT_EQ(obj->getProp2(), 42U);
}

/// @test
/// Throws from readOrAdapt when the member id is absent from the writer schema.
TEST(RemoteObject, ReadOrAdapt_MissingPropertyInWriterSchema_Throws)
{
  WorkQueue queue(50, false);

  sen::ClassSpec spec;
  spec.name = "EmptySchema";
  spec.qualifiedName = "pkg.EmptySchema";
  auto customSchema = sen::ClassType::make(spec);

  auto info = createTestInfo(&queue);
  info.writerSchema = customSchema;
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  const std::vector<uint8_t> buffer = {0, 0, 0, 0};
  sen::InputStream in(sen::makeConstSpan(buffer.data(), buffer.size()));
  uint32_t val;

  EXPECT_ANY_THROW({ obj->readOrAdapt(in, val, MemberHash {123U}, *sen::UInt32Type::get(), false); });
}

/// @test
/// Survives dispatching a writer event that carries fewer arguments than the local receiver
/// declares, no out-of-bounds read occurs.
/// @requirements(SEN-351)
TEST(RemoteObject, EventDispatch_FiltersMismatchArgs)
{
  WorkQueue queue(50, false);
  sen::CallableSpec writerSpec;
  writerSpec.name = "testEv";
  writerSpec.args.push_back(sen::Arg {"arg1", {}, sen::StringType::get()});
  writerSpec.transportMode = TransportMode::multicast;
  auto writerSchema = sen::ClassType::make(sen::ClassSpec {
    "Writer", "pkg.Writer", "", {}, {}, {sen::EventSpec {writerSpec}}, {}, {}, false, false, nullptr, nullptr});
  const auto* writerEvent = writerSchema->searchEventByName("testEv");

  auto info = createTestInfo(&queue);
  info.writerSchema = writerSchema;
  const auto obj = std::make_shared<TestRemoteObject>(std::move(info));

  const sen::impl::EventBuffer<std::string, uint32_t> evBuffer;

  std::vector readerArgs = {sen::Arg {"arg1", {}, sen::StringType::get()},
                            sen::Arg {"arg2", {}, sen::UInt32Type::get()}};

  const std::vector<uint8_t> buffer;

  obj->dispatchEventFromStream(evBuffer,
                               false,
                               readerArgs,
                               writerEvent->getId(),
                               sen::TimeStamp {0},
                               sen::ObjectId {1},
                               TransportMode::multicast,
                               sen::makeConstSpan(buffer),
                               obj.get());

  SUCCEED();
}

/// @test
/// Discards the pending callback when the object is destroyed before a response arrives, the
/// callback never fires.
/// @requirements(SEN-351)
TEST(RemoteObject, SafelyDropsPendingCallsOnDestruction)
{
  WorkQueue queue(50, false);
  queue.enable();

  bool callbackInvoked = false;

  {
    auto cb = MethodCallback<void>(&queue, [&](const auto&, const auto&) { callbackInvoked = true; });
    auto info =
      createTestInfo(&queue, [](auto...) -> sen::Result<CallId, std::string> { return sen::Ok(CallId {42U}); });
    const auto obj = std::make_shared<TestRemoteObject>(std::move(info));
    obj->makeRemoteCall<void>(MemberHash {123U}, TransportMode::confirmed, std::move(cb));
  }

  std::ignore = queue.executeAll();

  EXPECT_FALSE(callbackInvoked);
}
