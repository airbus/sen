// === runtime.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_RUNTIME_COMPATIBILITY_RUNTIME_3_SRC_RUNTIME_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_RUNTIME_COMPATIBILITY_RUNTIME_3_SRC_RUNTIME_H

// test_helpers
#include "test_helpers/tester.stl.h"

// sen
#include "sen/kernel/component_api.h"

// generated code
#include "stl/runtime/runtime.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <bitset>

namespace sen::test::runtime
{

class TestClassImpl final: public TestClassBase
{
public:
  SEN_NOCOPY_NOMOVE(TestClassImpl)

public:
  using TestClassBase::TestClassBase;
  ~TestClassImpl() override = default;

protected:  // TestClassBase
  std::string testMethodImpl(bool arg1, const TestStruct& arg2, u8 arg3) override;
  std::string testMethod2Impl(const TestStructBase& arg1, const TestEnum& arg2) override;
  void emitEventsImpl() override;
};

class TesterImpl final: public TesterBase
{
public:
  SEN_NOCOPY_NOMOVE(TesterImpl)

public:
  using TesterBase::TesterBase;
  ~TesterImpl() override = default;

public:  // NativeObject
  void registered(sen::kernel::RegistrationApi& api) override;

protected:  // TesterBase
  void doTestsImpl(std::promise<TestResult>&& promise) override;
  void checkLocalStateImpl(std::promise<TestResult>&& promise) override;
  void shutdownKernelImpl() override;

private:
  void checkRemote(u32 flagToSet);

private:
  std::shared_ptr<TestClassImpl> localObj_;
  std::shared_ptr<sen::Subscription<TestClassInterface>> remoteObjSub_;
  TestClassInterface* remoteObj_ = nullptr;
  std::promise<TestResult> promise_;
  std::bitset<15> localFlags_;
  std::bitset<17> remoteFlags_;
  bool testMethodPassed_ = false;
  bool testMethod2Passed_ = false;
  bool testEventPassed_ = true;
  std::shared_ptr<sen::Subscription<sen::kernel::KernelApiInterface>> kernelApiSub_;
  sen::kernel::KernelApiInterface* kernelApiObj_ = nullptr;
};

}  // namespace sen::test::runtime

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_RUNTIME_COMPATIBILITY_RUNTIME_3_SRC_RUNTIME_H
