// === transport.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_TRANSPORT_2_SRC_TRANSPORT_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_TRANSPORT_2_SRC_TRANSPORT_H

#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"
#include "stl/sen/kernel/kernel_objects.stl.h"
#include "stl/transport/transport.stl.h"

// test_helpers
#include "test_helpers/tester.stl.h"

// std
#include <bitset>
#include <future>

// spdlog
#include <spdlog/logger.h>

namespace sen::test
{

class TestClassImpl final: public TestClassBase
{
public:
  SEN_NOCOPY_NOMOVE(TestClassImpl)

public:
  using TestClassBase::TestClassBase;
  ~TestClassImpl() override = default;

protected:  // TestClassBase
  void testMethodImpl() const override;
  bool testMethod2Impl(u8 arg1, const std::string& arg2, f64 arg3) override;
  i32 testMethod3Impl(f32 arg1, u64 arg2) override;
  u16 testMethod4Impl(i16 arg1) override;
  std::string testMethod5Impl(bool arg1) const override;
  void emitEventsImpl() override;
  bool prop7AcceptsSet(i32 val) const override;
};

class TesterImpl final: public TesterBase
{
public:
  SEN_NOCOPY_NOMOVE(TesterImpl)

public:
  TesterImpl(std::string name, const sen::VarMap& args);
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
  std::bitset<7> localFlags_;
  std::bitset<11> remoteFlags_;
  bool testMethod2Passed_ = false;
  bool testMethod3Passed_ = false;
  bool testMethod4Passed_ = false;
  bool testMethod5Passed_ = false;
  bool testEventPassed_ = false;
  bool testEvent2Passed_ = false;
  std::shared_ptr<sen::Subscription<sen::kernel::KernelApiInterface>> kernelApiSub_;
  sen::kernel::KernelApiInterface* kernelApiObj_ = nullptr;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace sen::test

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_TRANSPORT_TRANSPORT_2_SRC_TRANSPORT_H
