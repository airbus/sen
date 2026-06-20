// === dispatcher_fixture.h =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_TEST_DISPATCHER_FIXTURE_H
#define SEN_COMPONENTS_JSONRPC_TEST_DISPATCHER_FIXTURE_H

// component
#include "dispatcher.h"
#include "messages.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/test_kernel.h"

// generated
#include "stl/fixture_widget.stl.h"

// nlohmann
#include "nlohmann/json.hpp"

// std
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::components::jsonrpc::test
{

/// In-process fixture for the dispatcher and kernel-side method handlers. Tests push JSON-RPC
/// frames onto the inbound queue and pop responses off the outbound queue, driving a real
/// `TestKernel` synchronously via `kernel.step()` (no TCP, no uWebSockets). The fixture publishes
/// a `FixtureWidget` named `widget` on `local.fixture` with a writable property, an event, and
/// a value-returning method. `WebSocketServer` is covered separately in `ws_server_test.cpp`.
class DispatcherFixture
{
  SEN_NOCOPY_NOMOVE(DispatcherFixture)

public:
  static constexpr const char* fixtureBusAddress = "local.fixture";
  static constexpr const char* widgetName = "widget";

public:
  DispatcherFixture();
  ~DispatcherFixture();

  /// Pushes a text frame onto the inbound queue. Does not step.
  void pushFrame(ConnectionId connId, std::string_view jsonText);

  /// Synthetic `ClientConnected` for `connId`. Drives `JsonRpcServer` registration on the next step.
  void connect(ConnectionId connId, std::string subject = "anonymous");

  /// Synthetic `ClientDisconnected` for `connId`. Tears down per-connection state on next step.
  void disconnect(ConnectionId connId);

  /// Synthetic backpressure update. `> 0` = high, `0` = low.
  void pushBackpressure(ConnectionId connId, std::size_t bufferedAmount);

  /// Direct access for tests that need to poke internal state. Safe between `step()` calls.
  [[nodiscard]] Dispatcher& dispatcher() noexcept;

  /// Steps the underlying TestKernel `count` times.
  void step(std::size_t count = 1);

  /// Pops the next outbound frame, or nullopt. Does not step. `pushBackRaw` frames come first.
  [[nodiscard]] std::optional<OutboundMessage> tryPopRaw();

  /// Drains everything currently available without stepping. Use before silence assertions.
  [[nodiscard]] std::vector<OutboundMessage> drainNow();

  /// Steps until a frame appears or `maxSteps` ticks pass; throws on exhaustion.
  [[nodiscard]] nlohmann::json popJsonAfterStepping(std::size_t maxSteps = 1024);

  /// Hands a frame back so the next pop returns it again. FIFO; preserve pop order to preserve
  /// producer ordering. The peek buffer is not partitioned by connection id, so frames stashed
  /// while filtering for connection A will also be returned to a subsequent pop on connection B.
  /// Tests that interleave pops across connections must drain or filter at the call site.
  void pushBackRaw(OutboundMessage msg);

  /// Each call runs in its own kernel tick, so back-to-back calls produce distinct commits -
  /// useful for tests that need to avoid same-tick coalescing on `setNextCounter`.
  void setWidgetCounter(int32_t value);

  /// Sets both writable properties from a single WorkQueue task; both writes land in the same
  /// kernel tick, exercising the per-object bundling path.
  void setWidgetCounterAndLabel(int32_t counterValue, std::string labelValue);

  /// Fires `chimed` with `message` and steps so the `eventTriggered` notification lands.
  void fireWidgetChimed(std::string message);

  /// Sets the variant-typed `mode` property. Tests use this to drive a propertyChanged whose
  /// value is a variant, exercising the outbound qualified-name wire-shape path end-to-end.
  void setWidgetModeBusy(std::string taskName);
  void setWidgetModeIdle(std::string reason);

  /// Removes / re-adds the widget instance under the same name. Active subscriptions drop on
  /// remove; the rewire path runs on add.
  void removeWidget();
  void addWidget();

  /// Publishes a fresh FixtureWidget `name` on `busAddress`. `getSource` is get-or-create, so
  /// this can bring a bus into existence mid-test — after an interest was already declared
  /// against that address (the late-bus / reconnect-shaped scenario).
  void publishWidgetOn(const char* busAddress, std::string name);

  /// Round-trips a probe `ping` on `connId` as a dispatcher-side barrier. Drains outbound traffic
  /// while polling, so callers should drain first if pre-barrier frames matter.
  void awaitDispatcherDrained(ConnectionId connId);

private:
  /// Pushes `body` onto the runner's WorkQueue and steps until it has executed. Definition lives
  /// in the .cpp; calling this from another TU will link-fail.
  template <typename Body>
  void pushOnWorkQueueAndStep(Body&& body);

  InboundQueue inboundQueue_;
  OutboundQueue outboundQueue_;
  std::deque<OutboundMessage> peekBuffer_;
  // Written by the kernel runner thread inside onInit / onRun, read on the test thread. The
  // happens-before for that hand-off comes from `onInit` running synchronously inside the
  // `TestKernel` ctor. Cleared inside onUnload so the destruction order below doesn't matter.
  std::shared_ptr<FixtureWidgetBase> widget_;
  std::shared_ptr<sen::ObjectSource> fixtureSource_;
  /// Widgets published via publishWidgetOn, each with the source keeping its bus alive.
  std::vector<std::pair<std::shared_ptr<sen::ObjectSource>, std::shared_ptr<FixtureWidgetBase>>> lateWidgets_;
  sen::kernel::RunApi* runApi_ {nullptr};
  std::unique_ptr<Dispatcher> dispatcher_;
  sen::impl::WorkQueue* workQueue_ {nullptr};
  // Member declaration order is destruction order in reverse. Kernel must be destroyed first
  // (its destructor drives onUnload, which clears the kernel-side state above); component must
  // outlive the kernel because the kernel holds a pointer to it.
  sen::kernel::TestComponent component_;
  std::unique_ptr<sen::kernel::TestKernel> kernel_;
};

}  // namespace sen::components::jsonrpc::test

#endif  // SEN_COMPONENTS_JSONRPC_TEST_DISPATCHER_FIXTURE_H
