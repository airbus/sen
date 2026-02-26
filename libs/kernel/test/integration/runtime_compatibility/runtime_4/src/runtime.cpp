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
#include "sen/core/base/compiler_macros.h"
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
#include <cmath>
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

const MaybeString& prop1()
{
  static const MaybeString prop1 = {"someString"};
  return prop1;
}

constexpr MaybeNative prop2 {4224U};
const MaybeSpeed& prop3()
{
  static const MaybeSpeed prop3 = {Speed {30}};
  return prop3;
}

const TestSequence prop4 = {1, 2, 3, 4, 5};
const TestSequence2& prop5()
{
  static const TestSequence2 prop5 = {10, 11, 12};
  return prop5;
}

constexpr TestStructChild prop6 = {3};
const Angle& prop7()
{
  static const Angle prop7 = {636.556};
  return prop7;
}

const Speed& prop8()
{
  static const Speed prop8 = {108};
  return prop8;
}

const Distance& prop9()
{
  static const Distance prop9 = {316.732};
  return prop9;
}

const Speed& prop10()
{
  static const Speed prop10 = {108};
  return prop10;
}

const std::string& prop11()
{
  static const std::string prop11 {"helloWorld"};
  return prop11;
}

const u64 prop12 = 4U;
constexpr bool prop13 = true;
const TestVariant prop14 {u8 {2}};
const TestVariant& prop15()
{
  static const TestVariant prop15 {"blue"};
  return prop15;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TestClassImpl
//--------------------------------------------------------------------------------------------------------------

std::string TestClassImpl::testMethodImpl(const runtime::MaybeBool& arg1, const runtime::TestStruct& arg2, i16 arg3)
{
  std::ignore = arg1;
  std::ignore = arg2;
  std::ignore = arg3;
  return "";
}

void TestClassImpl::emitEventsImpl()
{
#if SEN_GCC_VERSION_CHECK_SMALLER(12, 4, 0)
  // TODO (SEN-717): clean up with gcc12.4 on debian
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
  TestVariant tv {3.14};
#  pragma GCC diagnostic pop
#else
  // Note: if modified, patch line below
  TestVariant tv {3.14};
#endif

  testEvent(6, tv, "elem2", sen::Emit::onCommit);
}

SEN_EXPORT_CLASS(TestClassImpl)

//--------------------------------------------------------------------------------------------------------------
// TesterImpl
//--------------------------------------------------------------------------------------------------------------

void TesterImpl::registered(sen::kernel::RegistrationApi& api)
{
  // publish local object
  localObj_ = std::make_shared<TestClassImpl>("obj2");
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
          remoteObj_
            ->onTestEvent({this,
                           [this](const u8 arg1, const TestVariant arg3, const std::string& arg2)
                           {
                             testEventPassed_ = arg1 == 1 && arg2 == "elem2" && arg3 == TestVariant {"blue"};
                             checkRemote(16);
                           }})
            .keep();
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
  remoteObj_->setNextProp4(prop4);
  remoteObj_->setNextProp5(prop5());
  remoteObj_->setNextProp6(prop6);
  remoteObj_->setNextProp7(prop7());
  remoteObj_->setNextProp8(prop8());
  remoteObj_->setNextProp9(prop9());
  remoteObj_->setNextProp10(prop10());
  remoteObj_->setNextProp11(prop11());
  remoteObj_->setNextProp12(prop12);
  remoteObj_->setNextProp13(prop13);
  remoteObj_->setNextProp14(prop14);
  remoteObj_->setNextProp15(prop15());

  // call remote object methods
  remoteObj_->testMethod(MaybeBool(false),
                         TestStruct {5, 9},
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

  // command the remote object to emit events
  remoteObj_->emitEvents();
}

void TesterImpl::checkLocalStateImpl(std::promise<TestResult>&& promise)
{
  if (localFlags_.all())
  {
    // expected speed quantity value when converted from kph to mps
    MaybeSpeed expectedProp3 = {
      sen::Unit::convert(*sen::UnitRegistry::get().searchUnitByName("km_per_hour").value(),
                         *sen::UnitRegistry::get().searchUnitByName("meters_per_second").value(),
                         static_cast<float64_t>(prop3().value_or(Speed(0.0)).get()))};

    // check the properties of the local object
    if (checkProp<MaybeString>(promise, "prop1", localObj_->getProp1(), prop1()) &&
        checkProp<MaybeNative>(promise, "prop2", localObj_->getProp2(), prop2) &&
        checkProp<MaybeSpeed>(promise, "prop3", localObj_->getProp3(), expectedProp3) &&
        checkProp<TestSequence>(promise, "prop4", (localObj_->getProp4()), (prop4)) &&
        checkProp<TestSequence2>(promise, "prop5", localObj_->getProp5(), prop5()) &&
        checkProp<TestStructChild>(promise, "prop6", localObj_->getProp6(), prop6) &&
        checkProp<Angle>(promise, "prop7", localObj_->getProp7(), prop7()) &&
        checkProp<Speed>(promise, "prop8", localObj_->getProp8(), prop8()) &&
        checkProp<Distance>(promise, "prop9", localObj_->getProp9(), prop9()) &&
        checkProp<Speed>(promise, "prop10", localObj_->getProp10(), prop10()) &&
        checkProp<std::string>(promise, "prop11", localObj_->getProp11(), prop11()) &&
        checkProp<u64>(promise, "prop12", localObj_->getProp12(), prop12) &&
        checkProp<bool>(promise, "prop13", localObj_->getProp13(), prop13) &&
        checkProp<TestVariant>(promise, "prop14", localObj_->getProp14(), prop14) &&
        checkProp<TestVariant>(promise, "prop15", localObj_->getProp15(), prop15()))
    {
      promise.set_value(std::nullopt);
    }

    localFlags_ = 0;
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
  if (remoteFlags_.all())
  {
    // expected speed quantity value when converted from kph to mps
    MaybeSpeed expectedProp3 = {
      sen::Unit::convert(*sen::UnitRegistry::get().searchUnitByName("km_per_hour").value(),
                         *sen::UnitRegistry::get().searchUnitByName("meters_per_second").value(),
                         static_cast<float64_t>(prop3().value_or(Speed(0.0)).get()))};
    // approximated value of remote quantity when converted from mps to kph
    Speed valueProp10 =
      round(sen::Unit::convert(*sen::UnitRegistry::get().searchUnitByName("meters_per_second").value(),
                               *sen::UnitRegistry::get().searchUnitByName("km_per_hour").value(),
                               static_cast<float64_t>(Speed(30).get())));

    if (checkProp<MaybeString>(promise_, "prop1", remoteObj_->getProp1(), prop1()) &&
        checkProp<MaybeNative>(promise_, "prop2", remoteObj_->getProp2(), prop2) &&
        checkProp<MaybeSpeed>(promise_, "prop3", remoteObj_->getProp3(), expectedProp3) &&
        checkProp<TestSequence>(promise_, "prop4", (remoteObj_->getProp4()), (prop4)) &&
        checkProp<TestSequence2>(promise_, "prop5", remoteObj_->getProp5(), prop5()) &&
        checkProp<TestStructChild>(promise_, "prop6", remoteObj_->getProp6(), prop6) &&
        checkProp<Angle>(promise_, "prop7", remoteObj_->getProp7(), prop7()) &&
        checkProp<Speed>(promise_, "prop8", remoteObj_->getProp8(), prop8()) &&
        checkProp<Distance>(promise_, "prop9", remoteObj_->getProp9(), prop9()) &&
        checkProp<Speed>(promise_, "prop10", valueProp10, prop10()) &&
        checkProp<std::string>(promise_, "prop11", remoteObj_->getProp11(), prop11()) &&
        checkProp<u64>(promise_, "prop12", remoteObj_->getProp12(), prop12) &&
        checkProp<bool>(promise_, "prop13", remoteObj_->getProp13(), prop13) &&
        checkProp<TestVariant>(promise_, "prop14", remoteObj_->getProp14(), prop14) &&
        checkProp<TestVariant>(promise_, "prop15", remoteObj_->getProp15(), prop15()) &&
        checkMember(promise_, "testMethod", testMethodPassed_) && checkMember(promise_, "testEvent", testEventPassed_))
    {
      promise_.set_value(std::nullopt);
    }

    remoteFlags_ = 0;
  }
}

SEN_EXPORT_CLASS(TesterImpl)

}  // namespace sen::test::runtime
