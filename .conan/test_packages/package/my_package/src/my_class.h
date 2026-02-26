// === my_class.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef TEST_PACKAGES_TEST_UTIL_MY_PACKAGE_SRC_MY_CLASS_H
#define TEST_PACKAGES_TEST_UTIL_MY_PACKAGE_SRC_MY_CLASS_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/my_package/my_class.stl.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <cstdint>
#include <future>
#include <memory>
#include <string>

namespace my_package
{

class MyClassImpl: public MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(MyClassImpl)

public:
  MyClassImpl(const std::string& name, const sen::VarMap& args);
  ~MyClassImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override;
  void update(sen::kernel::RunApi& runApi) override;
  void someLocalMethod() override;

protected:
  int32_t addNumbersImpl(int32_t a, int32_t b) override;
  std::string echoImpl(const std::string& message) override;
  void changePropsImpl() override;
  bool prop7AcceptsSet(int32_t val) const override;
  void doingSomethingDeferredImpl(std::promise<std::string>&& promise) override;
  void doingSomethingDeferredWithoutReturningImpl(std::promise<void>&& promise) override;

private:
  void updateProp5(sen::kernel::RunApi& runApi);
  void updateProp8(sen::kernel::RunApi& runApi);
  void updateProp10(sen::kernel::RunApi& runApi);
  void checkCycleCount(sen::kernel::RunApi& runApi);

private:
  uint64_t cycleCount_ = 0U;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace my_package

#endif  // TEST_PACKAGES_TEST_UTIL_MY_PACKAGE_SRC_MY_CLASS_H
