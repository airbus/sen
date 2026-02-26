// === runtime.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "runtime.h"

// test_helpers
#include "test_helpers/helpers.h"
#include "test_helpers/tester.stl.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/runtime/runtime.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace sen::test::runtime
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

const std::string& prop1()
{
  static const std::string prop1 = {"someString"};
  return prop1;
}

const u64 prop2 {4224U};
const Speed& prop3()
{
  static const Speed prop3 = 8;
  return prop3;
}

const TestSequence& prop4()
{
  static const TestSequence prop4 = {1, 2, 3, 4, 5};
  return prop4;
}

const TestSequence2& prop5()
{
  static const TestSequence2 prop5 = {10, 11, 12};
  return prop5;
}

constexpr TestStructBase prop6 = {3.0};
const Angle& prop7()
{
  static const Angle prop7 = {11.11};
  return prop7;
}

const Speed& prop8()
{
  static const Speed prop8 = {30.89};
  return prop8;
}

const Distance& prop9()
{
  static const Distance prop9 = {96.54};
  return prop9;
}

const MaybeSpeed& prop10()
{
  static const MaybeSpeed prop10 = {Speed {30}};
  return prop10;
}

const MaybeString& prop11()
{
  static const MaybeString prop11 {"helloWorld"};
  return prop11;
}

const MaybeNative& prop12()
{
  static const MaybeNative prop12 = {4U};
  return prop12;
}

constexpr MaybeBool prop13 = {true};
const TestVariant prop14 {TestEnum::elem3};
const TestVariant& prop15()
{
  static const TestVariant prop15 {"blue"};
  return prop15;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TestClassImpl
//--------------------------------------------------------------------------------------------------------------

std::string TestClassImpl::testMethodImpl(const bool arg1, const TestStruct& arg2, const u8 arg3)
{
  std::ignore = arg1;
  std::ignore = arg2;
  std::ignore = arg3;
  return "";
}

std::string TestClassImpl::testMethod2Impl(const TestStructBase& arg1, const TestEnum& arg2)
{
  std::ignore = arg2;
  std::ignore = arg1;
  return "";
}

void TestClassImpl::emitEventsImpl() { testEvent(1.41, TestEnum::elem2, TestVariant {"blue"}, sen::Emit::onCommit); }

SEN_EXPORT_CLASS(TestClassImpl)

//--------------------------------------------------------------------------------------------------------------
// TesterImpl
//--------------------------------------------------------------------------------------------------------------

void TesterImpl::registered(sen::kernel::RegistrationApi& api)
{
  // publish local object
  localObj_ = std::make_shared<TestClassImpl>("obj1");
  api.getSource("session.bus")->add(localObj_);

  // detect kernel api
  kernelApiSub_ = api.selectAllFrom<sen::kernel::KernelApiInterface>("local.kernel");
  std::ignore = kernelApiSub_->list.onAdded(
    [this](const auto& iterators)
    {
      for (auto it = iterators.typedBegin; it != iterators.typedEnd; ++it)
      {
        kernelApiObj_ = *it;
      }
    });

  // local object callbacks
  localObj_->onProp1Changed({this, [this]() { localFlags_.set(0); }}).keep();
  localObj_->onProp2Changed({this, [this]() { localFlags_.set(1); }}).keep();
  localObj_->onProp3Changed({this, [this]() { localFlags_.set(2); }}).keep();
  localObj_->onProp4Changed({this, [this]() { localFlags_.set(3); }}).keep();
  localObj_->onProp5Changed({this, [this]() { localFlags_.set(4); }}).keep();
  localObj_->onProp6Changed({this, [this]() { localFlags_.set(5); }}).keep();
  localObj_->onProp7Changed({this, [this]() { localFlags_.set(6); }}).keep();
  localObj_->onProp8Changed({this, [this]() { localFlags_.set(7); }}).keep();
  localObj_->onProp9Changed({this, [this]() { localFlags_.set(8); }}).keep();
  localObj_->onProp10Changed({this, [this]() { localFlags_.set(9); }}).keep();
  localObj_->onProp11Changed({this, [this]() { localFlags_.set(10); }}).keep();
  localObj_->onProp12Changed({this, [this]() { localFlags_.set(11); }}).keep();
  localObj_->onProp13Changed({this, [this]() { localFlags_.set(12); }}).keep();
  localObj_->onProp14Changed({this, [this]() { localFlags_.set(13); }}).keep();
  localObj_->onProp15Changed({this, [this]() { localFlags_.set(14); }}).keep();

  // detect remote object
  remoteObjSub_ = api.selectAllFrom<TestClassInterface>("session.bus");
  std::ignore = remoteObjSub_->list.onAdded(
    [this](const auto& iterators)
    {
      for (auto it = iterators.typedBegin; it != iterators.typedEnd; ++it)
      {
        // ensure the object is remote
        if ((*it)->asObject().getId() != localObj_->getId())
        {
          remoteObj_ = *it;
          setNextReady(true);

          // remote object callbacks
          remoteObj_->onProp1Changed({this, [this]() { checkRemote(0); }}).keep();
          remoteObj_->onProp2Changed({this, [this]() { checkRemote(1); }}).keep();
          remoteObj_->onProp3Changed({this, [this]() { checkRemote(2); }}).keep();
          remoteObj_->onProp4Changed({this, [this]() { checkRemote(3); }}).keep();
          remoteObj_->onProp5Changed({this, [this]() { checkRemote(4); }}).keep();
          remoteObj_->onProp6Changed({this, [this]() { checkRemote(5); }}).keep();
          remoteObj_->onProp7Changed({this, [this]() { checkRemote(6); }}).keep();
          remoteObj_->onProp8Changed({this, [this]() { checkRemote(7); }}).keep();
          remoteObj_->onProp9Changed({this, [this]() { checkRemote(8); }}).keep();
          remoteObj_->onProp10Changed({this, [this]() { checkRemote(9); }}).keep();
          remoteObj_->onProp11Changed({this, [this]() { checkRemote(10); }}).keep();
          remoteObj_->onProp12Changed({this, [this]() { checkRemote(11); }}).keep();
          remoteObj_->onProp13Changed({this, [this]() { checkRemote(12); }}).keep();
          remoteObj_->onProp14Changed({this, [this]() { checkRemote(13); }}).keep();
          remoteObj_->onProp15Changed({this, [this]() { checkRemote(14); }}).keep();
          break;
        }
      }
    });
}

void TesterImpl::doTestsImpl(std::promise<TestResult>&& promise)
{
  promise_ = std::move(promise);
  // set values in the remote object
  remoteObj_->setNextProp1(prop1());
  remoteObj_->setNextProp2(prop2);
  remoteObj_->setNextProp3(prop3());
  remoteObj_->setNextProp4(prop4());
  remoteObj_->setNextProp5(prop5());
  remoteObj_->setNextProp6(prop6);
  remoteObj_->setNextProp7(prop7());
  remoteObj_->setNextProp8(prop8());
  remoteObj_->setNextProp9(prop9());
  remoteObj_->setNextProp10(prop10());
  remoteObj_->setNextProp11(prop11());
  remoteObj_->setNextProp12(prop12());
  remoteObj_->setNextProp13(prop13);
  remoteObj_->setNextProp14(prop14);
  remoteObj_->setNextProp15(prop15());

  // call remote object methods
  remoteObj_->testMethod(true,
                         {8, 3},
                         4,
                         {this,
                          [this](const auto& response)
                          {
                            if (response.isOk())
                            {
                              testMethodPassed_ = response.getValue() == "";
                            }
                            checkRemote(15);
                          }});

  remoteObj_->testMethod2({3.14},
                          TestEnum::elem5,
                          {this,
                           [this](const auto& response)
                           {
                             testMethod2Passed_ = response.isError();
                             checkRemote(16);
                           }});

  // command the remote object to emit events
  remoteObj_->emitEvents();
}

void TesterImpl::checkLocalStateImpl(std::promise<TestResult>&& promise)
{
  if (localFlags_.all())
  {
    // expected speed quantity value when converted from mps to kph
    MaybeSpeed expectedProp10 = {
      sen::Unit::convert(*sen::UnitRegistry::get().searchUnitByName("meters_per_second").value(),
                         *sen::UnitRegistry::get().searchUnitByName("km_per_hour").value(),
                         prop10().value_or(0.0))};

    // check the properties of the local object
    if (checkProp<std::string>(promise, "prop1", localObj_->getProp1(), prop1()) &&
        checkProp<u64>(promise, "prop2", localObj_->getProp2(), prop2) &&
        checkProp<Speed>(promise, "prop3", localObj_->getProp3(), prop3()) &&
        checkProp<TestSequence>(promise, "prop4", (localObj_->getProp4()), (prop4())) &&
        checkProp<TestSequence2>(promise, "prop5", localObj_->getProp5(), prop5()) &&
        checkProp<TestStructBase>(promise, "prop6", localObj_->getProp6(), prop6) &&
        checkProp<Angle>(promise, "prop7", localObj_->getProp7(), prop7()) &&
        checkProp<Speed>(promise, "prop8", localObj_->getProp8(), prop8()) &&
        checkProp<Distance>(promise, "prop9", localObj_->getProp9(), prop9()) &&
        checkProp<MaybeSpeed>(promise, "prop10", localObj_->getProp10(), expectedProp10) &&
        checkProp<MaybeString>(promise, "prop11", localObj_->getProp11(), prop11()) &&
        checkProp<MaybeNative>(promise, "prop12", localObj_->getProp12(), prop12()) &&
        checkProp<MaybeBool>(promise, "prop13", localObj_->getProp13(), prop13) &&
        checkProp<TestVariant>(promise, "prop14", localObj_->getProp14(), prop14) &&
        checkProp<TestVariant>(promise, "prop15", localObj_->getProp15(), prop15()))
    {
      promise.set_value(std::nullopt);
    }

    localFlags_ = 0;
    testMethodPassed_ = false;
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

void TesterImpl::checkRemote(const u32 flagToSet)
{
  remoteFlags_.set(flagToSet);
  if (remoteFlags_.all())
  {
    // expected speed quantity value when converted from mps to kph
    MaybeSpeed expectedProp10 = {
      sen::Unit::convert(*sen::UnitRegistry::get().searchUnitByName("meters_per_second").value(),
                         *sen::UnitRegistry::get().searchUnitByName("km_per_hour").value(),
                         prop10().value_or(0.0))};

    // approximated value of remote quantity when converted from kph to mps
    Speed valueProp3 = sen::Unit::convert(*sen::UnitRegistry::get().searchUnitByName("km_per_hour").value(),
                                          *sen::UnitRegistry::get().searchUnitByName("meters_per_second").value(),
                                          Speed(30));

    if (checkProp<std::string>(promise_, "prop1", remoteObj_->getProp1(), prop1()) &&
        checkProp<u64>(promise_, "prop2", remoteObj_->getProp2(), prop2) &&
        checkProp<Speed>(promise_, "prop3", valueProp3, prop3()) &&
        checkProp<TestSequence>(promise_, "prop4", (remoteObj_->getProp4()), (prop4())) &&
        checkProp<TestSequence2>(promise_, "prop5", remoteObj_->getProp5(), prop5()) &&
        checkProp<TestStructBase>(promise_, "prop6", remoteObj_->getProp6(), prop6) &&
        checkProp<Angle>(promise_, "prop7", remoteObj_->getProp7(), prop7()) &&
        checkProp<Speed>(promise_, "prop8", remoteObj_->getProp8(), prop8()) &&
        checkProp<Distance>(promise_, "prop9", remoteObj_->getProp9(), prop9()) &&
        checkProp<MaybeSpeed>(promise_, "prop10", remoteObj_->getProp10(), expectedProp10) &&
        checkProp<MaybeString>(promise_, "prop11", remoteObj_->getProp11(), prop11()) &&
        checkProp<MaybeNative>(promise_, "prop12", remoteObj_->getProp12(), prop12()) &&
        checkProp<MaybeBool>(promise_, "prop13", remoteObj_->getProp13(), prop13) &&
        checkProp<TestVariant>(promise_, "prop14", remoteObj_->getProp14(), prop14) &&
        checkProp<TestVariant>(promise_, "prop15", remoteObj_->getProp15(), prop15()) &&
        checkMember(promise_, "testMethod", testMethodPassed_) &&
        checkMember(promise_, "extraMethod", testMethod2Passed_))
    {
      promise_.set_value(std::nullopt);
    }

    remoteFlags_ = 0;
  }
}

SEN_EXPORT_CLASS(TesterImpl)

}  // namespace sen::test::runtime
