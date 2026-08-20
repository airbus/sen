// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/hash32.h"
#include "sen/core/io/util.h"
#include "sen/core/obj/object_list.h"
#include "sen/kernel/component_api.h"

// generated code (our own)
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// generated code (jsonrpc - typed StaticFileServerInterface + StaticBundle shape)
#include "stl/static_file_server.stl.h"

// compressed frontend bundle, baked in at build time
#include "generated/explorer_html.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <memory>
#include <utility>

namespace sen::components::webexplorer
{

namespace
{

constexpr auto defaultControlBusName = "jsonrpc_control";
constexpr auto explorerUrlPrefix = "/explorer";
constexpr auto indexFileName = "index.html";
constexpr auto cycleTime = sen::Duration::fromHertz(0.1F);

[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger()
{
  return kernel::KernelApi::getOrCreateLogger("webexplorer");
}

[[nodiscard]] sen::components::jsonrpc::StaticBundle makeFrontendBundle()
{
  sen::components::jsonrpc::StaticBundle bundle;
  bundle.urlPrefix = explorerUrlPrefix;
  bundle.indexFileName = indexFileName;

  sen::components::jsonrpc::StaticFile file;
  file.path = indexFileName;
  file.contentType = "text/html";

  const auto body = sen::decompressSymbolToString(explorer_html, explorer_htmlSize);
  if (body.empty())
  {
    getLogger()->warn("frontend bundle is empty; no UI will be served");
  }
  file.contents.assign(body.begin(), body.end());

  bundle.files.push_back(std::move(file));
  return bundle;
}

}  // namespace

class WebExplorerComponent: public kernel::Component
{
  SEN_NOCOPY_NOMOVE(WebExplorerComponent)

public:
  WebExplorerComponent() = default;
  ~WebExplorerComponent() override = default;

public:
  [[nodiscard]] kernel::FuncResult run(kernel::RunApi& api) override
  {
    const auto config = toValue<Configuration>(api.getConfig());
    const std::string controlBusName = config.controlBusName.value_or(defaultControlBusName);
    getLogger()->info("attaching to local.{} for jsonrpc StaticFileServer", controlBusName);

    sen::components::jsonrpc::StaticFileServerInterface* staticFileServer = nullptr;

    auto subscription = api.selectAllFrom<sen::components::jsonrpc::StaticFileServerInterface>(
      kernel::BusAddress {"local", controlBusName},
      [&](const auto& added)
      {
        for (auto* server: added)
        {
          if (!staticFileServer)
          {
            staticFileServer = server;
            server->registerStaticBundle(makeFrontendBundle());
            getLogger()->info("registered frontend bundle at '{}'", explorerUrlPrefix);
          }
          else
          {
            getLogger()->error(
              "more than one StaticFileServer on local.{}; the bundle the explorer "
              "serves is non-deterministic across reloads. Configure exactly one "
              "StaticFileServer per bus.",
              controlBusName);
          }
        }
      },
      [&](const auto& removed)
      {
        if (staticFileServer)
        {
          for (auto* server: removed)
          {
            if (server == staticFileServer)
            {
              staticFileServer = nullptr;
              getLogger()->info("StaticFileServer detached");
              break;
            }
          }
        }
      });

    return api.execLoop(cycleTime, [] {}, false);
  }
};

}  // namespace sen::components::webexplorer

SEN_COMPONENT(sen::components::webexplorer::WebExplorerComponent)
