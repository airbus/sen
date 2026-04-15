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
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/runtime/runtime.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <future>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

namespace sen::test::runtime
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

const TestSequence prop1 = {20U, 21U, 22U, 23U, 24U};
const TestSequence2 prop2 = {3.141519, 2.618033};
const TestSequence prop3 = {1U, 2U, 3U, 4U, 5U};
const TestSequence2 prop4 = {3.141519, 2.618033};
const Frequency& prop5()
{
  static const Frequency prop5 = {80.55};
  return prop5;
}

const Angle& prop6()
{
  static const Angle prop6 = {0.67};
  return prop6;
}

const Frequency& prop7()
{
  static const Frequency prop7 = {70.88};
  return prop7;
}

const Angle& prop8()
{
  static const Angle prop8 = {0.88};
  return prop8;
}

constexpr TestStruct prop9 {60U, 70U};
constexpr EmptyStruct prop10 {true};
constexpr TestStructBase prop11 {5U};
constexpr TestStructChild prop12 {{}, 1.12, 0.67, -33};
constexpr TestStruct prop13 {3U, 4U};
constexpr TestVariant prop14 = TestVariant {std::in_place_index<4>, TestStruct {31U, 33U}};
const TestEnum prop15 = TestEnum::elem2;

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TestClassImpl
//--------------------------------------------------------------------------------------------------------------

i16 TestClassImpl::testMethodImpl(const runtime::TestSequence& arg1,
                                  const runtime::MaybeVariant& arg2,
                                  const runtime::TestStructChild& arg3)
{
  std::ignore = arg1;
  std::ignore = arg2;
  std::ignore = arg3;
  return 0;
}

void TestClassImpl::emitEventsImpl() { testEvent(1.41, MaybeString {"elem2"}, i16 {4}, sen::Emit::onCommit); }

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
  kernelApiSub_ = api.selectAllFrom<sen::kernel::KernelApiInterface>(
    "local.kernel", [this](const auto& addedObjects) { kernelApiObj_ = *addedObjects.begin(); });

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
  remoteObj_->setNextProp1(prop1);
  remoteObj_->setNextProp2(prop2);
  remoteObj_->setNextProp3(prop3);
  remoteObj_->setNextProp4(prop4);
  remoteObj_->setNextProp5(prop5());
  remoteObj_->setNextProp6(prop6());
  remoteObj_->setNextProp7(prop7());
  remoteObj_->setNextProp8(prop8());
  remoteObj_->setNextProp9(prop9);
  remoteObj_->setNextProp10(prop10);
  remoteObj_->setNextProp11(prop11);
  remoteObj_->setNextProp12(prop12);
  remoteObj_->setNextProp13(prop13);
  remoteObj_->setNextProp14(prop14);
  remoteObj_->setNextProp15(prop15);

  // call remote object methods
  remoteObj_->testMethod({1, 2, 3, 4, 5},
                         {TestEnum::elem4},
                         {-5, 1.75, 32.324, -14},
                         {this,
                          [this](const auto& response)
                          {
                            if (response.isOk())
                            {
                              testMethodPassed_ = response.getValue() == 0;
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
    // check the properties of the local object
    if (checkProp<TestSequence>(promise, "prop1", localObj_->getProp1(), prop1) &&
        checkProp<TestSequence2>(promise, "prop2", localObj_->getProp2(), prop2) &&
        checkProp<TestSequence>(promise, "prop3", localObj_->getProp3(), prop3) &&
        checkProp<TestSequence2>(promise, "prop4", localObj_->getProp4(), prop4) &&
        checkProp<Frequency>(promise, "prop5", localObj_->getProp5(), prop5()) &&
        checkProp<Angle>(promise, "prop6", localObj_->getProp6(), prop6()) &&
        checkProp<Frequency>(promise, "prop7", localObj_->getProp7(), prop7()) &&
        checkProp<Angle>(promise, "prop8", localObj_->getProp8(), prop8()) &&
        checkProp<TestStruct>(promise, "prop9", localObj_->getProp9(), prop9) &&
        checkProp<EmptyStruct>(promise, "prop10", localObj_->getProp10(), prop10) &&
        checkProp<TestStructBase>(promise, "prop11", localObj_->getProp11(), prop11) &&
        checkProp<TestStructChild>(promise, "prop12", localObj_->getProp12(), prop12) &&
        checkProp<TestStruct>(promise, "prop13", localObj_->getProp13(), prop13) &&
        checkProp<TestVariant>(promise, "prop14", localObj_->getProp14(), prop14) &&
        checkProp<TestEnum>(promise, "prop15", localObj_->getProp15(), prop15))
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

void TesterImpl::checkRemote(u32 flagToSet)
{
  remoteFlags_.set(flagToSet);

  if (remoteFlags_.all())
  {
    if (checkProp<TestSequence>(promise_, "prop1", remoteObj_->getProp1(), prop1) &&
        checkProp<TestSequence2>(promise_, "prop2", remoteObj_->getProp2(), prop2) &&
        checkProp<TestSequence>(promise_, "prop3", remoteObj_->getProp3(), prop3) &&
        checkProp<TestSequence2>(promise_, "prop4", remoteObj_->getProp4(), prop4) &&
        checkProp<Frequency>(promise_, "prop5", remoteObj_->getProp5(), prop5()) &&
        checkProp<Angle>(promise_, "prop6", remoteObj_->getProp6(), prop6()) &&
        checkProp<Frequency>(promise_, "prop7", remoteObj_->getProp7(), prop7()) &&
        checkProp<Angle>(promise_, "prop8", remoteObj_->getProp8(), prop8()) &&
        checkProp<TestStruct>(promise_, "prop9", remoteObj_->getProp9(), prop9) &&
        checkProp<EmptyStruct>(promise_, "prop10", remoteObj_->getProp10(), prop10) &&
        checkProp<TestStructBase>(promise_, "prop11", remoteObj_->getProp11(), prop11) &&
        checkProp<TestStructChild>(promise_, "prop12", remoteObj_->getProp12(), prop12) &&
        checkProp<TestStruct>(promise_, "prop13", remoteObj_->getProp13(), prop13) &&
        checkProp<TestVariant>(promise_, "prop14", remoteObj_->getProp14(), prop14) &&
        checkProp<TestEnum>(promise_, "prop15", remoteObj_->getProp15(), prop15) &&
        checkMember(promise_, "testMethod", testMethodPassed_))
    {
      promise_.set_value(std::nullopt);
    }

    remoteFlags_ = 0;
  }
}

SEN_EXPORT_CLASS(TesterImpl)

}  // namespace sen::test::runtime
