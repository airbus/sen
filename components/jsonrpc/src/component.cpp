// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// local
#include "auth.h"
#include "dispatcher.h"
#include "messages.h"
#include "static_file_server.h"
#include "ws_server.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/base/scope_guard.h"
#include "sen/core/io/util.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/configuration.stl.h"

namespace sen::components::jsonrpc
{

namespace
{

constexpr auto defaultControlBusName = "jsonrpc_control";
constexpr auto staticFileServerName = "static_file_server";

}  // namespace

class JsonRpcComponent: public kernel::Component
{
  SEN_NOCOPY_NOMOVE(JsonRpcComponent)

public:
  JsonRpcComponent() = default;
  ~JsonRpcComponent() override = default;

public:
  [[nodiscard]] kernel::FuncResult run(kernel::RunApi& api) override
  {
    const auto config = toValue<Configuration>(api.getConfig());

    // TLS is reserved for a future release; accepting it here would silently start a plain
    // server.
    if (config.tls.has_value())
    {
      return Err(
        kernel::ExecError {kernel::ErrorCategory::ioError,
                           "JsonRpcComponent: Configuration.tls is reserved for a future release; unset it to start"});
    }

    const std::string controlBusName = config.controlBusName.value_or(defaultControlBusName);
    auto controlBus = api.getSource(kernel::BusAddress {"local", controlBusName});
    if (!controlBus)
    {
      return Err(kernel::ExecError {kernel::ErrorCategory::ioError,
                                    "JsonRpcComponent: failed to acquire control bus 'local." + controlBusName + "'"});
    }

    InboundQueue inboundQueue;
    OutboundQueue outboundQueue;
    NoAuth authenticator;

    auto serverResult = WebSocketServer::make(config.address,
                                              config.port,
                                              inboundQueue,
                                              outboundQueue,
                                              authenticator,
                                              config.connectionLimits.value_or(ConnectionLimits {}));
    if (serverResult.isError())
    {
      return Err(kernel::ExecError {kernel::ErrorCategory::ioError, serverResult.getError()});
    }
    auto server = std::move(serverResult).getValue();

    Dispatcher dispatcher {inboundQueue, outboundQueue, api};

    auto staticFileServer = std::make_shared<StaticFileServer>(staticFileServerName, server.get());
    controlBus->add(staticFileServer);
    auto staticFileGuard = sen::makeScopeGuard([&]() { controlBus->remove(staticFileServer); });

    constexpr auto defaultUpdateFreqHz = 60.0;
    const auto cycleTime = Duration::fromHertz(config.updateFreqHz.value_or(defaultUpdateFreqHz));

    return api.execLoop(
      cycleTime,
      [&]()
      {
        dispatcher.processBatch();
        server->notifyOutbound();
      },
      false);
  }
};

}  // namespace sen::components::jsonrpc

SEN_COMPONENT(sen::components::jsonrpc::JsonRpcComponent)
