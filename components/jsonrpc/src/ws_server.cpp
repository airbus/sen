// === ws_server.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "ws_server.h"

// local
#include "auth.h"
#include "messages.h"
#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/result.h"

// generated
#include "stl/configuration.stl.h"
#include "stl/jsonrpc.stl.h"

// uWebSockets
#include <App.h>
#include <HttpParser.h>
#include <HttpResponse.h>
#include <PerMessageDeflate.h>
#include <WebSocket.h>
#include <WebSocketProtocol.h>
#include <libusockets.h>

// moodycamel
#include <concurrentqueue.h>

// spdlog
#include <spdlog/spdlog.h>

// std
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen::components::jsonrpc
{

using sen::std_util::checkedConversion;

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

// Tuning knobs.
/// Bulk-dequeue chunk size for the outbound drain loop (not a per-tick throughput cap: the
/// loop in `makeDrainAction` keeps pulling chunks until the queue is empty). Tuned for
/// moodycamel `try_dequeue_bulk` amortization vs stack-frame size.
constexpr std::size_t outboundDrainBatchSize = 32U;

// Default uWS per-socket limits, used when the configuration leaves them unset.
constexpr uint16_t defaultIdleTimeoutSeconds = 120U;
constexpr uint32_t defaultMaxPayloadBytes = 16U * 1024U;
constexpr uint32_t defaultMaxBackpressureBytes = 64U * 1024U;
constexpr uint32_t defaultHighBackpressureBytes = 32U * 1024U;

// WebSocket close codes from RFC 6455 section 7.4.1.
constexpr int wsCloseUnsupportedData = 1003;
constexpr int wsCloseServerError = 1011;

//--------------------------------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------------------------------

struct PerSocketData
{
  ConnectionId id {0};
  bool isHigh {false};
  Identity identity;
};
using Connection = uWS::WebSocket<false, true, PerSocketData>;

/// State owned by the uWS thread for the lifetime of `app.run()`.
// Plain state bag: everything in it belongs to the one uWS thread, and the constructor
// below only wires the references. Accessors would add nothing here.
// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct LoopRuntime
{
  uWS::App app;
  us_listen_socket_t* listenSocket {nullptr};
  std::unordered_map<ConnectionId, Connection*> connectionsById;
  ConnectionId nextId {0U};
  InboundQueue& inboundQueue;
  OutboundQueue& outboundQueue;
  moodycamel::ConsumerToken outboundConsumerToken;
  const Authenticator& authenticator;
  ConnectionLimits limits;

  LoopRuntime(InboundQueue& inbound, OutboundQueue& outbound, const Authenticator& auth, ConnectionLimits lim) noexcept
    : inboundQueue(inbound)
    , outboundQueue(outbound)
    , outboundConsumerToken(outbound)
    , authenticator(auth)
    , limits(std::move(lim))
  {
  }
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

//--------------------------------------------------------------------------------------------------------------
// Backpressure decisions
//--------------------------------------------------------------------------------------------------------------

struct BackpressureDecision
{
  bool nextHigh {false};
  std::optional<BackpressureUpdate> emit;
};

/// Edge-only decision after `ws->send()`: emits a `BackpressureUpdate` only on the low->high transition.
[[nodiscard]] BackpressureDecision decideOnSent(bool prevHigh,
                                                std::size_t bufferedAmount,
                                                std::size_t highThresholdBytes)
{
  const bool nowHigh = bufferedAmount >= highThresholdBytes;
  if (nowHigh && !prevHigh)
  {
    return {true, BackpressureUpdate {bufferedAmount}};
  }
  return {nowHigh, std::nullopt};
}

/// Edge-only decision when uWS fires `drain`: emits a zero `BackpressureUpdate` only on the high->low transition.
[[nodiscard]] BackpressureDecision decideOnDrained(bool prevHigh)
{
  if (prevHigh)
  {
    return {false, BackpressureUpdate {0}};
  }
  return {false, std::nullopt};
}

//--------------------------------------------------------------------------------------------------------------
// Loop-thread setup helpers
//--------------------------------------------------------------------------------------------------------------

void installBehavior(LoopRuntime& rt)
{
  uWS::App::WebSocketBehavior<PerSocketData> behavior {};
  behavior.compression = uWS::DISABLED;
  behavior.maxPayloadLength = rt.limits.maxPayloadBytes.value_or(defaultMaxPayloadBytes);
  behavior.idleTimeout = rt.limits.idleTimeoutSeconds.value_or(defaultIdleTimeoutSeconds);
  behavior.maxBackpressure = rt.limits.maxBackpressureBytes.value_or(defaultMaxBackpressureBytes);

  behavior.upgrade = [&rt](auto* res, auto* req, auto* context)
  {
    auto identityResult = rt.authenticator.verify(req->getHeader("authorization"));
    if (identityResult.isError())
    {
      SPDLOG_LOGGER_WARN(
        getLogger(), "WebSocketServer: rejecting WebSocket upgrade: {}", std::move(identityResult).getError());
      res->writeStatus("401 Unauthorized")->end();
      return;
    }
    PerSocketData userData {};
    userData.identity = std::move(identityResult).getValue();
    res->template upgrade<PerSocketData>(std::move(userData),
                                         req->getHeader("sec-websocket-key"),
                                         req->getHeader("sec-websocket-protocol"),
                                         req->getHeader("sec-websocket-extensions"),
                                         context);
  };

  behavior.open = [&rt](Connection* ws)
  {
    ++rt.nextId;
    auto* data = ws->getUserData();
    data->id = rt.nextId;
    rt.connectionsById.emplace(rt.nextId, ws);
    getLogger()->debug("WebSocketServer: connection {} opened (subject={})", rt.nextId.get(), data->identity.subject);
    rt.inboundQueue.enqueue(InboundMessage {data->id, ClientConnected {std::move(data->identity)}});
  };

  behavior.message = [&rt](Connection* ws, std::string_view msg, uWS::OpCode op)
  {
    if (op != uWS::OpCode::TEXT)
    {
      const auto id = ws->getUserData()->id;
      SPDLOG_LOGGER_WARN(getLogger(),
                         "WebSocketServer: connection {} sent a non-text frame (opcode {}); closing {}",
                         id.get(),
                         static_cast<int>(op),
                         wsCloseUnsupportedData);
      ws->end(wsCloseUnsupportedData, "binary frames not supported");
      return;
    }
    rt.inboundQueue.enqueue(InboundMessage {ws->getUserData()->id, std::string {msg}});
  };

  behavior.drain = [&rt](Connection* ws)
  {
    auto* data = ws->getUserData();
    const auto decision = decideOnDrained(data->isHigh);
    data->isHigh = decision.nextHigh;
    if (decision.emit.has_value())
    {
      rt.inboundQueue.enqueue(InboundMessage {data->id, decision.emit.value()});
    }
  };

  behavior.close = [&rt](Connection* ws, int /*code*/, std::string_view /*message*/)
  {
    const auto id = ws->getUserData()->id;
    rt.connectionsById.erase(id);
    getLogger()->debug("WebSocketServer: connection {} closed", id.get());
    rt.inboundQueue.enqueue(InboundMessage {id, ClientDisconnected {}});
  };

  rt.app.ws<PerSocketData>("/*", std::move(behavior));
}

[[nodiscard]] bool startListening(LoopRuntime& rt, const std::string& address, uint16_t port, uint16_t& boundPortOut)
{
  bool listening = false;
  rt.app.listen(address,
                port,
                [&rt, &listening, &boundPortOut](us_listen_socket_t* ls)
                {
                  if (ls == nullptr)
                  {
                    return;
                  }
                  rt.listenSocket = ls;
                  // The usockets C API hands the listen socket out as us_listen_socket_t but takes
                  // us_socket_t; the cast is the documented way to use it.
                  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                  const int actualPort = us_socket_local_port(0, reinterpret_cast<struct us_socket_t*>(ls));
                  if (actualPort > 0)
                  {
                    boundPortOut = checkedConversion<uint16_t>(actualPort);
                  }
                  listening = true;
                });
  return listening;
}

[[nodiscard]] std::function<void()> makeShutdownAction(LoopRuntime& rt)
{
  return [&rt]()
  {
    if (rt.listenSocket != nullptr)
    {
      us_listen_socket_close(0, rt.listenSocket);
      rt.listenSocket = nullptr;
    }

    std::vector<Connection*> snapshot;
    snapshot.reserve(rt.connectionsById.size());

    for (auto& [id, ws]: rt.connectionsById)
    {
      snapshot.push_back(ws);
    }

    for (auto* ws: snapshot)
    {
      ws->close();
    }
  };
}

[[nodiscard]] std::function<void(std::string, HttpGetHandler)> makeHttpGetRegistrar(LoopRuntime& rt)
{
  return [&rt](std::string pattern, HttpGetHandler handler)
  {
    rt.app.get(std::move(pattern),
               [h = std::move(handler)](uWS::HttpResponse<false>* res, uWS::HttpRequest* req) { h(res, req); });
  };
}

[[nodiscard]] std::function<void()> makeDrainAction(LoopRuntime& rt)
{
  return [&rt]()
  {
    std::array<OutboundMessage, outboundDrainBatchSize> batch;
    while (true)
    {
      const std::size_t count =
        rt.outboundQueue.try_dequeue_bulk(rt.outboundConsumerToken, batch.begin(), batch.size());
      if (count == 0)
      {
        break;
      }

      for (std::size_t i = 0; i < count; ++i)
      {
        auto it = rt.connectionsById.find(batch.at(i).connectionId);
        if (it == rt.connectionsById.end())
        {
          continue;
        }

        auto* ws = it->second;
        const auto status = ws->send(batch.at(i).payload, uWS::OpCode::TEXT);
        auto* data = ws->getUserData();

        if (status == Connection::SendStatus::DROPPED)
        {
          SPDLOG_LOGGER_WARN(
            getLogger(),
            "WebSocketServer: connection {} send DROPPED at hard backpressure cap ({} bytes buffered); closing {}",
            data->id.get(),
            ws->getBufferedAmount(),
            wsCloseServerError);
          ws->end(wsCloseServerError, "outbound buffer overflow");
          continue;
        }
        const auto decision = decideOnSent(data->isHigh,
                                           ws->getBufferedAmount(),
                                           rt.limits.highBackpressureBytes.value_or(defaultHighBackpressureBytes));
        data->isHigh = decision.nextHigh;
        if (decision.emit.has_value())
        {
          rt.inboundQueue.enqueue(InboundMessage {data->id, decision.emit.value()});
        }
      }
    }
  };
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// WebSocketServer
//--------------------------------------------------------------------------------------------------------------

sen::Result<std::unique_ptr<WebSocketServer>, std::string> WebSocketServer::make(std::string address,
                                                                                 uint16_t port,
                                                                                 InboundQueue& inboundQueue,
                                                                                 OutboundQueue& outboundQueue,
                                                                                 const Authenticator& authenticator,
                                                                                 ConnectionLimits limits)
{
  // The soft trigger must fire before the hard cap, otherwise uWS closes the connection at the
  // hard cap before the dispatcher ever sees the soft signal.
  const auto maxBp = limits.maxBackpressureBytes.value_or(defaultMaxBackpressureBytes);
  const auto highBp = limits.highBackpressureBytes.value_or(defaultHighBackpressureBytes);
  if (highBp >= maxBp)
  {
    return sen::Err("WebSocketServer: highBackpressureBytes (" + std::to_string(highBp) +
                    ") must be < maxBackpressureBytes (" + std::to_string(maxBp) + ")");
  }

  auto server = std::make_unique<WebSocketServer>(
    std::move(address), port, inboundQueue, outboundQueue, authenticator, std::move(limits), Private {});

  if (auto result = server->start(); result.isError())
  {
    return sen::Err(result.getError());
  }

  return sen::Ok(std::move(server));
}

WebSocketServer::WebSocketServer(std::string address,
                                 uint16_t port,
                                 InboundQueue& inboundQueue,
                                 OutboundQueue& outboundQueue,
                                 const Authenticator& authenticator,
                                 ConnectionLimits limits,
                                 Private notUsable)
  : address_(std::move(address))
  , requestedPort_(port)
  , inboundQueue_(inboundQueue)
  , outboundQueue_(outboundQueue)
  , authenticator_(authenticator)
  , limits_(std::move(limits))
{
  std::ignore = notUsable;
}

WebSocketServer::~WebSocketServer()
{
  if (!thread_.joinable())
  {
    return;
  }
  SPDLOG_LOGGER_INFO(getLogger(), "WebSocketServer: stopping (port={})", boundPort_);

  if (auto* loopSnapshot = loop_.exchange(nullptr, std::memory_order_acq_rel);
      loopSnapshot != nullptr && shutdownAction_)
  {
    loopSnapshot->defer(std::move(shutdownAction_));
  }
  thread_.join();
}

void WebSocketServer::notifyOutbound()
{
  auto* loop = loop_.load(std::memory_order_acquire);
  if (loop == nullptr || !drainAction_)
  {
    return;
  }

  auto copy = drainAction_;
  loop->defer(std::move(copy));
}

void WebSocketServer::registerHttpGet(std::string pattern, HttpGetHandler handler)
{
  auto* loop = loop_.load(std::memory_order_acquire);
  if (loop == nullptr || !httpGetRegistrar_)
  {
    SPDLOG_LOGGER_WARN(
      getLogger(), "WebSocketServer::registerHttpGet: server not started yet, dropping route '{}'", pattern);
    return;
  }
  loop->defer([reg = httpGetRegistrar_, p = std::move(pattern), h = std::move(handler)]() mutable
              { reg(std::move(p), std::move(h)); });
}

sen::Result<void, std::string> WebSocketServer::start()
{
  std::promise<bool> ready;
  auto readyFuture = ready.get_future();

  thread_ = std::thread(&WebSocketServer::runUwsLoop, this, std::move(ready));

  if (!readyFuture.get())
  {
    thread_.join();
    return sen::Err("WebSocketServer: failed to bind to " + address_ + ":" + std::to_string(requestedPort_));
  }

  return sen::Ok();
}

void WebSocketServer::runUwsLoop(std::promise<bool> readyPromise)
{
  LoopRuntime rt {inboundQueue_, outboundQueue_, authenticator_, limits_};

  installBehavior(rt);
  const bool listening = startListening(rt, address_, requestedPort_, boundPort_);
  shutdownAction_ = makeShutdownAction(rt);
  drainAction_ = makeDrainAction(rt);
  httpGetRegistrar_ = makeHttpGetRegistrar(rt);

  if (listening)
  {
    SPDLOG_LOGGER_INFO(
      getLogger(), "WebSocketServer: listening on {}:{}", address_.empty() ? "0.0.0.0" : address_, boundPort_);
  }

  loop_.store(uWS::Loop::get(), std::memory_order_release);
  readyPromise.set_value(listening);
  if (!listening)
  {
    return;
  }
  rt.app.run();
}

uint16_t WebSocketServer::getPort() const noexcept { return boundPort_; }

}  // namespace sen::components::jsonrpc
