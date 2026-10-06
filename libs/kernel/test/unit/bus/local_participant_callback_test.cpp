// === local_participant_callback_test.cpp =============================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// kernel
#include "bus/local_participant.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "test_kernel/stl/my_class.stl.h"

// google test
#include <gtest/gtest.h>

// spdlog
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

// std
#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using sen::kernel::impl::errorAddFromCallback;
using sen::kernel::impl::errorRemoveFromCallback;

// Helper class to capture kernel error logs
class KernelLogCapture
{
  class ErrorSink: public spdlog::sinks::base_sink<std::mutex>
  {
  public:
    [[nodiscard]] bool contains(std::string_view text)
    {
      std::lock_guard lock(mutex_);

      return std::any_of(
        errors_.begin(), errors_.end(), [&](const auto& line) { return line.find(text) != std::string::npos; });
    }

  protected:
    void sink_it_(const spdlog::details::log_msg& message) override
    {
      if (message.level == spdlog::level::err)
      {
        errors_.emplace_back(message.payload.data(), message.payload.size());
      }
    }
    void flush_() override
    {
      // Left blank intentionally
    }

  private:
    std::vector<std::string> errors_;
  };

public:
  SEN_NOCOPY_NOMOVE(KernelLogCapture)

public:
  KernelLogCapture() { sen::kernel::KernelApi::getOrCreateLogger("kernel")->sinks().push_back(sink_); }

  ~KernelLogCapture()
  {
    auto& sinks = sen::kernel::KernelApi::getOrCreateLogger("kernel")->sinks();
    sinks.erase(std::remove(sinks.begin(), sinks.end(), sink_), sinks.end());
  }

  [[nodiscard]] ErrorSink& sink() { return *sink_; }

private:
  std::shared_ptr<ErrorSink> sink_ = std::make_shared<ErrorSink>();
};

class TestClass: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(TestClass)

public:
  using MyClassBase::MyClassBase;
  ~TestClass() override = default;

  using MyClassBase::somethingHappened;
};

TEST(LocalParticipantCallback, addFromCallbackReported)
{
  KernelLogCapture capture;

  auto trigger = std::make_shared<TestClass>("trigger", sen::VarMap {});
  auto extra = std::make_shared<TestClass>("extra", sen::VarMap {});
  std::shared_ptr<sen::ObjectSource> source;
  sen::kernel::TestComponent component;

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source = api.getSource("local.test");
      source->add(trigger);

      return sen::kernel::done();
    });

  component.onRun(
    [&](auto& api)
    {
      bool added = false;
      trigger
        ->onSomethingHappened({api.getWorkQueue(),
                               [&]()
                               {
                                 if (!added)
                                 {
                                   source->add(extra);
                                   added = true;
                                 }
                               }})
        .keep();

      return api.execLoop(std::chrono::seconds(1), [&]() { trigger->somethingHappened(); });
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(4U);

  EXPECT_TRUE(capture.sink().contains(errorAddFromCallback));
}

TEST(LocalParticipantCallback, removeFromCallbackReported)
{
  KernelLogCapture capture;

  auto trigger = std::make_shared<TestClass>("trigger", sen::VarMap {});
  auto extra = std::make_shared<TestClass>("extra", sen::VarMap {});
  std::shared_ptr<sen::ObjectSource> source;
  sen::kernel::TestComponent component;

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source = api.getSource("local.test");
      source->add(trigger);
      source->add(extra);
      return sen::kernel::done();
    });

  component.onRun(
    [&](auto& api)
    {
      bool removed = false;
      trigger
        ->onSomethingHappened({api.getWorkQueue(),
                               [&]()
                               {
                                 if (!removed)
                                 {
                                   source->remove(extra);
                                   removed = true;
                                 }
                               }})
        .keep();

      return api.execLoop(std::chrono::seconds(1), [&]() { trigger->somethingHappened(); });
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(4U);

  EXPECT_TRUE(capture.sink().contains(errorRemoveFromCallback));
}

}  // namespace
