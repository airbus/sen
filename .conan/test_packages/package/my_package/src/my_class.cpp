// === my_class.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "my_class.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/version.h"  // NOLINT(misc-include-cleaner)
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/tracer.h"

// generated code
#include "stl/my_package/basic_types.stl.h"
#include "stl/my_package/config.stl.h"
#include "stl/my_package/my_class.stl.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

// std
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace my_package
{

MyClassImpl::MyClassImpl(const std::string& name, const sen::VarMap& args): MyClassBase(name, args)
{
  logger_ = spdlog::get("my_logger");
  if (!logger_)
  {
    logger_ = std::make_shared<spdlog::logger>("my_logger");
  }

  logger_->info("MyClassImpl with name \"" + name + "\" created");

  setNextProp6({0.3, 0.4, 0.5, 0.5, 0.5, 0.3, 0.4, 0.5, 0.5, 0.5});

  setNextProp9(9);
}

void MyClassImpl::registered(sen::kernel::RegistrationApi& api)
{
  auto config = api.getConfig();

  auto freq = config["freqHz"].get<int64_t>();
  logger_->info("component freqHz = {}", freq);

  auto userConfig = sen::toValue<Configuration>(api.getConfig());
  std::cout << "MyComponentWithConfig config = \n" << userConfig << "\n";
}

bool MyClassImpl::prop7AcceptsSet(i32 val) const
{
  return val >= -5 && val <= 5;  // just as an example, let's say we accept a range [-5, 5]
}

void MyClassImpl::update(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());
  logger_->trace("updating (time: {})", runApi.getTime().toLocalString());

  updateProp5(runApi);
  updateProp8(runApi);
  updateProp10(runApi);
  checkCycleCount(runApi);

  // this is to test what happens when throwing an exception
  if (getThrowException())
  {
    sen::throwRuntimeError("some error");
  }
}

void MyClassImpl::updateProp5(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  auto nextProp5 = getProp5() + 1;
  setNextProp5(nextProp5);

  // we can fetch the value we just set...
  if (getNextProp5() != nextProp5)
  {
    sen::throwRuntimeError("something went wrong with prop5");
  }

  // but the current still remains
  if (getProp5() == nextProp5)
  {
    sen::throwRuntimeError("values should be different!");
  }
}

void MyClassImpl::updateProp8(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  auto prop8 = getProp8();
  if (prop8 < sen::QuantityTraits<MetersU16>::max)
  {
    setNextProp8(prop8 + 1);
  }
}

void MyClassImpl::updateProp10(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  if (cycleCount_ % 10 == 0)
  {
    auto prop10 = getProp10();

    if (prop10)
    {
      if (prop10.value() > 0)
      {
        setNextProp10(-10.0f);
      }
      else
      {
        setNextProp10(std::nullopt);
      }
    }
    else
    {
      setNextProp10(10.0f);
    }
  }
}

void MyClassImpl::checkCycleCount(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  auto maxCycleCount = getIterationCount();
  if (maxCycleCount != 0 && cycleCount_ == maxCycleCount)
  {
    runApi.requestKernelStop(0);
  }

  cycleCount_++;
}

void MyClassImpl::someLocalMethod()
{
  // nothing to do
}

int32_t MyClassImpl::addNumbersImpl(int32_t a, int32_t b) { return a + b; }

std::string MyClassImpl::echoImpl(const std::string& message) { return message; }

void MyClassImpl::changePropsImpl()
{
  Vec2 val = getProp4();
  val.x += 0.5f;  // simply make some changes
  val.y += 1.5;   // to a property, to see the effect
  setNextProp4(val);
}

void MyClassImpl::doingSomethingDeferredImpl(std::promise<std::string>&& promise)
{
  promise.set_value("Doing something deferred");
}

void MyClassImpl::doingSomethingDeferredWithoutReturningImpl(std::promise<void>&& promise) { promise.set_value(); }

SEN_EXPORT_CLASS(MyClassImpl)

}  // namespace my_package
