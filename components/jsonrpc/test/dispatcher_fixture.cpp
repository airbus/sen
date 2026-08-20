// === dispatcher_fixture.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "dispatcher_fixture.h"

// local
#include "dispatcher.h"
#include "messages.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// generated
#include "stl/fixture_widget.stl.h"
#include "stl/jsonrpc.stl.h"

// std
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::components::jsonrpc::test
{

namespace
{

constexpr auto cycleHz = 200.0F;
// Upper bound for any one mutator's step() loop. ~5ms per tick gives roughly 5s of headroom.
constexpr std::size_t maxStepsPerMutator = 1000;
constexpr std::size_t maxStepsForProbe = 1000;

class FixtureWidgetImpl: public FixtureWidgetBase
{
  SEN_NOCOPY_NOMOVE(FixtureWidgetImpl)

public:
  using FixtureWidgetBase::FixtureWidgetBase;
  ~FixtureWidgetImpl() override = default;

protected:
  i32 doubledImpl(i32 value) override { return value * 2; }
  void boomImpl() override { throw std::runtime_error("boom"); }
};

}  // namespace

DispatcherFixture::DispatcherFixture()
{
  component_.onInit(
    [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      widget_ = std::make_shared<FixtureWidgetImpl>(widgetName);
      fixtureSource_ = api.getSource(fixtureBusAddress);
      fixtureSource_->add(widget_);
      workQueue_ = api.getWorkQueue();
      return sen::kernel::done();
    });

  component_.onRun(
    [this](sen::kernel::RunApi& api) -> sen::kernel::FuncResult
    {
      if (!dispatcher_)
      {
        dispatcher_ = std::make_unique<Dispatcher>(inboundQueue_, outboundQueue_, api);
      }
      runApi_ = &api;
      return api.execLoop(sen::Duration::fromHertz(cycleHz), [this]() { dispatcher_->processBatch(); }, false);
    });

  // Drop the dispatcher and the widget while the kernel's session manager is still alive;
  // letting their dtors run after kernel teardown would deref a destroyed mutex.
  component_.onUnload(
    [this](sen::kernel::UnloadApi&& /*api*/) -> sen::kernel::FuncResult
    {
      dispatcher_.reset();
      if (fixtureSource_ && widget_)
      {
        fixtureSource_->remove(widget_);
      }
      for (auto& [source, lateWidget]: lateWidgets_)
      {
        source->remove(lateWidget);
      }
      lateWidgets_.clear();
      widget_.reset();
      fixtureSource_.reset();
      workQueue_ = nullptr;
      runApi_ = nullptr;
      return sen::Ok();
    });

  kernel_ = std::make_unique<sen::kernel::TestKernel>(&component_);
  // One step to defeat the startup race against the runner thread: `workQueue_.enable()` runs
  // inside the runner's `doRun()`, after the TestKernel ctor returns. A mutator push that lands
  // before enable() would be silently discarded.
  kernel_->step();
}

DispatcherFixture::~DispatcherFixture()
{
  // Drive `onUnload` before any other member's destructor runs.
  kernel_.reset();
}

template <typename Body>
void DispatcherFixture::pushOnWorkQueueAndStep(Body&& body)
{
  if (workQueue_ == nullptr)
  {
    throw std::runtime_error("DispatcherFixture: workQueue_ not set (kernel not initialized?)");
  }
  std::atomic_bool done {false};
  workQueue_->push(
    [body = std::forward<Body>(body), &done]() mutable
    {
      body();
      done.store(true, std::memory_order_release);
    },
    true);
  for (std::size_t i = 0; i < maxStepsPerMutator; ++i)
  {
    kernel_->step();
    if (done.load(std::memory_order_acquire))
    {
      return;
    }
  }
  throw std::runtime_error("DispatcherFixture: kernel never executed mutator task");
}

void DispatcherFixture::pushFrame(ConnectionId connId, std::string_view jsonText)
{
  inboundQueue_.enqueue(InboundMessage {connId, std::string {jsonText}});
}

void DispatcherFixture::connect(ConnectionId connId, std::string subject)
{
  inboundQueue_.enqueue(InboundMessage {connId, ClientConnected {Identity {std::move(subject)}}});
}

void DispatcherFixture::disconnect(ConnectionId connId)
{
  inboundQueue_.enqueue(InboundMessage {connId, ClientDisconnected {}});
}

void DispatcherFixture::pushBackpressure(ConnectionId connId, std::size_t bufferedAmount)
{
  inboundQueue_.enqueue(InboundMessage {connId, BackpressureUpdate {bufferedAmount}});
}

Dispatcher& DispatcherFixture::dispatcher() noexcept { return *dispatcher_; }

void DispatcherFixture::setWidgetCounter(int32_t value)
{
  pushOnWorkQueueAndStep(
    [this, value]
    {
      if (widget_)
      {
        widget_->setNextCounter(value);
      }
    });
}

void DispatcherFixture::setWidgetCounterAndLabel(int32_t counterValue, std::string labelValue)
{
  pushOnWorkQueueAndStep(
    [this, counterValue, labelValue = std::move(labelValue)]
    {
      if (widget_)
      {
        widget_->setNextCounter(counterValue);
        widget_->setNextLabel(labelValue);
      }
    });
}

void DispatcherFixture::fireWidgetChimed(std::string message)
{
  pushOnWorkQueueAndStep(
    [this, message = std::move(message)]
    {
      if (widget_)
      {
        widget_->chimed(message);
      }
    });
}

void DispatcherFixture::setWidgetModeBusy(std::string taskName)
{
  pushOnWorkQueueAndStep(
    [this, taskName = std::move(taskName)]
    {
      if (widget_)
      {
        widget_->setNextMode(FixtureMode {ModeBusy {std::move(taskName)}});
      }
    });
}

void DispatcherFixture::setWidgetModeIdle(std::string reason)
{
  pushOnWorkQueueAndStep(
    [this, reason = std::move(reason)]
    {
      if (widget_)
      {
        widget_->setNextMode(FixtureMode {ModeIdle {std::move(reason)}});
      }
    });
}

void DispatcherFixture::removeWidget()
{
  pushOnWorkQueueAndStep(
    [this]
    {
      if (fixtureSource_ && widget_)
      {
        fixtureSource_->remove(widget_);
      }
    });
}

void DispatcherFixture::addWidget()
{
  pushOnWorkQueueAndStep(
    [this]
    {
      if (fixtureSource_ && widget_)
      {
        fixtureSource_->add(widget_);
      }
    });
}

void DispatcherFixture::publishWidgetOn(const char* busAddress, std::string name)
{
  pushOnWorkQueueAndStep(
    [this, busAddress, name = std::move(name)]
    {
      auto lateWidget = std::make_shared<FixtureWidgetImpl>(name);
      auto source = runApi_->getSource(busAddress);
      source->add(lateWidget);
      lateWidgets_.emplace_back(std::move(source), std::move(lateWidget));
    });
}

void DispatcherFixture::awaitDispatcherDrained(ConnectionId connId)
{
  // Probe ids live in their own range so they can't collide with test-chosen ids.
  static std::atomic<int> probeId {900'000};
  const int id = probeId.fetch_add(1);
  pushFrame(connId, std::string {R"({"jsonrpc":"2.0","method":"ping","id":)"} + std::to_string(id) + "}");

  for (std::size_t i = 0; i < maxStepsForProbe; ++i)
  {
    auto raw = tryPopRaw();
    if (!raw.has_value())
    {
      kernel_->step();
      continue;
    }
    try
    {
      const auto frame = nlohmann::json::parse(raw->payload);
      if (frame.value("id", -1) == id)
      {
        return;
      }
    }
    catch (const nlohmann::json::parse_error&)
    {
      // Not JSON - skip.
    }
  }
  throw std::runtime_error("DispatcherFixture::awaitDispatcherDrained: probe ping did not return");
}

void DispatcherFixture::step(std::size_t count) { kernel_->step(count); }

std::optional<OutboundMessage> DispatcherFixture::tryPopRaw()
{
  if (!peekBuffer_.empty())
  {
    auto msg = std::move(peekBuffer_.front());
    peekBuffer_.pop_front();
    return msg;
  }
  OutboundMessage msg;
  if (outboundQueue_.try_dequeue(msg))
  {
    return msg;
  }
  return std::nullopt;
}

std::vector<OutboundMessage> DispatcherFixture::drainNow()
{
  std::vector<OutboundMessage> out;
  while (auto msg = tryPopRaw())
  {
    out.push_back(std::move(*msg));
  }
  return out;
}

void DispatcherFixture::pushBackRaw(OutboundMessage msg) { peekBuffer_.push_back(std::move(msg)); }

nlohmann::json DispatcherFixture::popJsonAfterStepping(std::size_t maxSteps)
{
  for (std::size_t i = 0; i <= maxSteps; ++i)
  {
    if (auto raw = tryPopRaw())
    {
      return nlohmann::json::parse(raw->payload);
    }
    if (i < maxSteps)
    {
      kernel_->step();
    }
  }
  throw std::runtime_error("DispatcherFixture::popJsonAfterStepping: no outbound frame after " +
                           std::to_string(maxSteps) + " steps");
}

}  // namespace sen::components::jsonrpc::test
