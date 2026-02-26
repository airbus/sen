// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "local_terminal.h"
#include "printer.h"
#include "remote_terminal_server.h"
#include "shell.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/obj/object_mux.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/shell.stl.h"

// asio
#include <asio/ip/tcp.hpp>

// std
#include <memory>
#include <tuple>
#include <utility>

namespace sen::components::shell
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr auto* implName = "shell_impl";
constexpr auto* remoteName = "remote";
constexpr auto* internalSession = "local";
constexpr auto* internalBus = "shell";
constexpr auto defaultPort = RemoteTerminalServer::defaultPort;
constexpr auto defaultBufferStyle = BufferStyle::hexdump;
constexpr auto defaultTimeStyle = TimeStyle::timestampUTC;
constexpr auto defaultShellUpdateFreq = Duration::fromHertz(10.0);

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// ShellComponent
//--------------------------------------------------------------------------------------------------------------

struct ShellComponent: public kernel::Component
{
  [[nodiscard]] kernel::PassResult init(kernel::InitApi&& api) override
  {
    // get the configuration
    config_.bufferStyle = defaultBufferStyle;
    config_.timeStyle = defaultTimeStyle;
    config_.serverPort = config_.serverPort == 0 ? defaultPort : config_.serverPort;
    config_.noLogo = false;
    config_.logBus = "local.log";
    VariantTraits<Configuration>::variantToValue(api.getConfig(), config_);

    if (config_.prompt.empty() && !api.getAppName().empty())
    {
      config_.prompt = api.getAppName();
    }

    return done();
  }

  [[nodiscard]] kernel::FuncResult run(kernel::RunApi& api) override
  {
    auto shellSource = api.getSource(kernel::BusAddress {internalSession, internalBus});
    return config_.serverEnabled ? runServer(shellSource, api) : runLocal(shellSource, api);
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }

private:
  [[nodiscard]] kernel::FuncResult runLocal(std::shared_ptr<ObjectSource> shellSource, kernel::RunApi& api)
  {
    ObjectMux mux;

    // create the local terminal
    auto localTerm = std::make_unique<LocalTerminal>();
    localTerm->enableRawMode();

    // print the logo
    if (!config_.noLogo && localTerm->isARealTerminal())
    {
      Printer::printWelcome(localTerm.get());
    }
    auto impl = std::make_shared<ShellImpl>(implName, api, mux, api.getTypes(), config_, localTerm.get());
    shellSource->add(impl);

    return api.execLoop(defaultShellUpdateFreq, nullptr, false);
  }

  [[nodiscard]] kernel::FuncResult runServer(std::shared_ptr<ObjectSource> shellSource, kernel::RunApi& api)
  {
    std::shared_ptr<ShellImpl> impl;
    std::shared_ptr<RemoteTerminal> terminal;

    ObjectMux mux;

    auto createRemote = [this, &impl, &terminal, &shellSource, &api, &mux](auto& io, asio::ip::tcp::socket socket)
    {
      std::ignore = io;

      // only allow one remote connection
      if (impl)
      {
        socket.close();
        return;
      }

      terminal = std::make_unique<RemoteTerminal>(std::move(socket),
                                                  [&impl, &shellSource]()
                                                  {
                                                    shellSource->remove(impl);
                                                    impl.reset();
                                                  });

      if (!config_.noLogo)
      {
        Printer::printWelcome(terminal.get());
      }

      impl = std::make_shared<ShellImpl>(remoteName, api, mux, api.getTypes(), config_, terminal.get());
      shellSource->add(impl);
    };

    auto server = std::make_unique<RemoteTerminalServer>(config_.serverPort, std::move(createRemote));
    return api.execLoop(defaultShellUpdateFreq, nullptr, false);
  }

private:
  Configuration config_;
};

}  // namespace sen::components::shell

SEN_COMPONENT(sen::components::shell::ShellComponent)
