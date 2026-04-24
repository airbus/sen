// === transport.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "transport.h"

#include "test_helpers/helpers.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/kernel_objects.stl.h"
#include "stl/transport/transport.stl.h"

// spdlog
#include <spdlog/sinks/stdout_color_sinks.h>

// std
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace sen::test
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr u32 prop1 = 4U;
constexpr i64 prop2 = 5;
constexpr f32 prop3 = 1.45f;
constexpr f64 prop4 = -67.3;
const sen::Duration prop5 {30.5};
const std::string& prop6()
{
  static const std::string prop6 = "stringExample";
  return prop6;
}

const i32 prop7 = 405;

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TestClassImpl
//--------------------------------------------------------------------------------------------------------------
void TestClassImpl::testMethodImpl() const {}

bool TestClassImpl::testMethod2Impl(u8 arg1, const std::string& arg2, f64 arg3)
{
  std::ignore = arg1;
  std::ignore = arg2;
  std::ignore = arg3;

  return true;
}

i32 TestClassImpl::testMethod3Impl(f32 arg1, u64 arg2)
{
  std::ignore = arg2;

  return static_cast<i32>(arg1);
}

u16 TestClassImpl::testMethod4Impl(i16 arg1)
{
  std::ignore = arg1;

  return 0;
}

std::string TestClassImpl::testMethod5Impl(bool arg1) const
{
  std::ignore = arg1;
  return "";
}

void TestClassImpl::emitEventsImpl()
{
  testEvent("testEvent", sen::TimeStamp {1}, sen::Duration {1}, sen::Emit::onCommit);
  testEvent2("testEvent2", sen::TimeStamp {2}, sen::Duration {2}, sen::Emit::onCommit);
}

bool TestClassImpl::prop7AcceptsSet(i32 val) const
{
  std::ignore = val;
  return true;
}

SEN_EXPORT_CLASS(TestClassImpl)

//--------------------------------------------------------------------------------------------------------------
// TesterImpl
//--------------------------------------------------------------------------------------------------------------

TesterImpl::TesterImpl(std::string name, const sen::VarMap& args)
  : TesterBase(std::move(name), args), logger_(spdlog::stdout_color_mt("transport_2"))
{
}

void TesterImpl::registered(sen::kernel::RegistrationApi& api)
{
  // publish local object
  localObj_ = std::make_shared<TestClassImpl>("obj2", prop3);
  api.getSource("session.bus")->add(localObj_);

  // detect kernel api
  kernelApiSub_ = api.selectAllFrom<sen::kernel::KernelApiInterface>(
    "local.kernel", [this](const auto& addedObjects) { kernelApiObj_ = *addedObjects.begin(); });

  // local object callbacks
  localObj_->onProp1Changed({this, [this]() { localFlags_.set(0); }}).keep();
  localObj_->onProp2Changed({this, [this]() { localFlags_.set(1); }}).keep();
  localObj_->onProp5Changed({this, [this]() { localFlags_.set(4); }}).keep();
  localObj_->onProp6Changed({this, [this]() { localFlags_.set(5); }}).keep();
  localObj_->onProp7Changed({this, [this]() { localFlags_.set(6); }}).keep();

  // detect remote object
  remoteObjSub_ = api.selectAllFrom<TestClassInterface>(
    "session.bus",
    [this](const auto& addedObjects)
    {
      for (auto elem: addedObjects)
      {
        // ensure the object is remote
        if (elem->asObject().getId() != localObj_->getId())
        {
          remoteObj_ = elem;
          setNextReady(true);

          // remote object callbacks
          remoteObj_->onProp1Changed({this, [this]() { checkRemote(0); }}).keep();
          remoteObj_->onProp2Changed({this, [this]() { checkRemote(1); }}).keep();
          remoteObj_->onProp5Changed({this, [this]() { checkRemote(2); }}).keep();
          remoteObj_->onProp6Changed({this, [this]() { checkRemote(3); }}).keep();
          remoteObj_->onProp7Changed({this, [this]() { checkRemote(4); }}).keep();

          remoteObj_
            ->onTestEvent({this,
                           [this](const std::string& arg1, const sen::TimeStamp arg2, const sen::Duration arg3)
                           {
                             testEventPassed_ =
                               arg1 == "testEvent" && arg2 == sen::TimeStamp(1) && arg3 == sen::Duration {1};
                             checkRemote(9);
                           }})
            .keep();

          remoteObj_
            ->onTestEvent2({this,
                            [this](const std::string& arg1, const sen::TimeStamp arg2, const sen::Duration arg3)
                            {
                              testEvent2Passed_ =
                                arg1 == "testEvent2" && arg2 == sen::TimeStamp(2) && arg3 == sen::Duration {2};
                              checkRemote(10);
                            }})
            .keep();
          break;
        }
      }
    });
}

void TesterImpl::doTestsImpl(std::promise<TestResult>&& promise)
{
  logger_->info("called doTestsImpl");

  promise_ = std::move(promise);

  // set writable properties values in the remote object
  remoteObj_->setNextProp1(prop1);
  remoteObj_->setNextProp2(prop2);
  remoteObj_->setNextProp6(prop6());
  remoteObj_->setNextProp7(prop7);

  // call remote object methods
  remoteObj_->testMethod();
  remoteObj_->testMethod2(8U,
                          "basicString",
                          84.3,
                          {this,
                           [this](const auto& response)
                           {
                             if (response.isOk())
                             {
                               testMethod2Passed_ = response.getValue() == true;
                             }
                             checkRemote(5);
                           }});
  remoteObj_->testMethod3(5.56f,
                          60U,
                          {this,
                           [this](const auto& response)
                           {
                             if (response.isOk())
                             {
                               testMethod3Passed_ = response.getValue() == static_cast<i32>(5.56f);
                             }
                             checkRemote(6);
                           }});
  remoteObj_->testMethod4(-10,
                          {this,
                           [this](const auto& response)
                           {
                             if (response.isOk())
                             {
                               testMethod4Passed_ = response.getValue() == 0U;
                             }
                             checkRemote(7);
                           }});
  remoteObj_->testMethod5(false,
                          {this,
                           [this](const auto& response)
                           {
                             if (response.isOk())
                             {
                               testMethod5Passed_ = response.getValue() == "";
                             }
                             checkRemote(8);
                           }});

  // emit the test event with the local object
  remoteObj_->emitEvents();
}

void TesterImpl::checkLocalStateImpl(std::promise<TestResult>&& promise)
{
  logger_->info("called checkLocalStateImpl");

  if (localFlags_.to_string() == "0000011")
  {
    // check the properties of the local object
    if (checkProp<u8>(promise, "prop1", localObj_->getProp1(), prop1) &&
        checkProp<i64>(promise, "prop2", localObj_->getProp2(), prop2) &&
        checkProp<f32>(promise, "prop3", localObj_->getProp3(), prop3) && !eq(localObj_->getProp4(), prop4) &&
        !eq(localObj_->getProp5(), prop5) && !eq(localObj_->getProp6(), prop6()) && !eq(localObj_->getProp7(), prop7))
    {
      promise.set_value(std::nullopt);
    }

    localFlags_ = 0;
    testMethod2Passed_ = false;
    testMethod3Passed_ = false;
    testMethod4Passed_ = false;
    testMethod5Passed_ = false;
    return;
  }
  // try again until all the local properties have been set
  checkLocalStateImpl(std::move(promise));
}

void TesterImpl::shutdownKernelImpl()
{
  if (kernelApiObj_ != nullptr)
  {
    kernelApiObj_->shutdown();
  }
}

void TesterImpl::checkRemote(u32 flagToSet)
{
  remoteFlags_.set(flagToSet);

  // all bits except those from static properties and un-matching method4 should be set
  if (remoteFlags_.to_string() == "11111100011")
  {
    // properties with bit=1 should have equal value with remote object. properties with bit=0 should not be equal.
    if (checkProp<u8>(promise_, "prop1", remoteObj_->getProp1(), prop1) &&
        checkProp<i64>(promise_, "prop2", remoteObj_->getProp2(), prop2) && !eq(remoteObj_->getProp3(), prop3) &&
        !eq(remoteObj_->getProp4(), prop4) && !eq(remoteObj_->getProp5(), prop5) &&
        checkProp<std::string>(promise_, "prop6", remoteObj_->getProp6(), prop6()) &&
        !eq(remoteObj_->getProp7(), prop7) && checkMember(promise_, "testMethod2", testMethod2Passed_) &&
        checkMember(promise_, "testMethod3", testMethod3Passed_) &&
        checkMember(promise_, "testMethod4", testMethod4Passed_) &&
        checkMember(promise_, "testMethod5", testMethod5Passed_) &&
        checkMember(promise_, "testEvent", testEventPassed_) && checkMember(promise_, "testEvent2", testEvent2Passed_))
    {
      promise_.set_value(std::nullopt);
    }

    remoteFlags_ = 0;
  }
}

SEN_EXPORT_CLASS(TesterImpl)

}  // namespace sen::test
