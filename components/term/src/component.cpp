// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// local
#include "app.h"
#include "banner.h"
#include "command_engine.h"
#include "completer.h"
#include "log_router.h"
#include "output_capture.h"
#include "theme.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/version.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/term.stl.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// spdlog
#include <spdlog/spdlog.h>

// std
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr auto* internalSession = "local";

/// Resolve the default persistent history file. Shared with the shell component so the user has
/// one history across both tools.
std::filesystem::path defaultHistoryPath()
{
#ifdef _WIN32
  const char* home = std::getenv("USERPROFILE");
#else
  const char* home = std::getenv("HOME");
#endif
  if (home == nullptr || *home == '\0')
  {
    return {};  // no HOME -> persistence disabled
  }
  return std::filesystem::path(home) / ".sen_history.txt";
}

constexpr auto* internalBus = "term";
constexpr auto defaultUpdateFreq = Duration::fromHertz(30.0);
const auto defaultCallTimeout = Duration(std::chrono::seconds(30));

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TermComponent
//--------------------------------------------------------------------------------------------------------------

struct TermComponent: public kernel::Component
{
  [[nodiscard]] kernel::PassResult init(kernel::InitApi&& api) override
  {
    config_.timeStyle = TimeStyle::utc;
    config_.theme = ThemeStyle::catppuccinMocha;
    config_.noLogo = false;
    config_.callTimeout = defaultCallTimeout;
    config_.logBus = "local.log";
    VariantTraits<Configuration>::variantToValue(api.getConfig(), config_);

    const char* envTheme = std::getenv("SEN_TERM_THEME");
    if (envTheme != nullptr && *envTheme != '\0')
    {
      const auto& enumType = *MetaTypeTrait<ThemeStyle>::meta();
      const auto* enumerator = enumType.getEnumFromName(std::string(envTheme));
      if (enumerator != nullptr)
      {
        config_.theme = static_cast<ThemeStyle>(enumerator->key);
      }
    }

    setActiveTheme(themeForStyle(config_.theme));

    return done();
  }

  [[nodiscard]] kernel::FuncResult run(kernel::RunApi& api) override
  {
    auto termSource = api.getSource(kernel::BusAddress {internalSession, internalBus});

    CommandEngine* enginePtr = nullptr;  // raw pointer; engine outlives app

    auto appMode = (config_.mode == DisplayMode::repl) ? App::Mode::repl : App::Mode::tui;
    auto app = std::make_unique<App>(
      [&enginePtr](const std::string& cmd)
      {
        if (enginePtr != nullptr)
        {
          enginePtr->execute(cmd);
        }
      },
      appMode);

    app->setHistoryFile(defaultHistoryPath());
    app->init();
    app->tick();  // draw the initial frame (alternate screen + welcome message)

    if (!config_.noLogo)
    {
      const auto& buildInfo = kernel::Kernel::getBuildInfo();
      app->appendOutput("");
      app->appendElement(
        renderBanner(SEN_VERSION_STRING, buildInfo.compiler, buildInfo.debugMode ? "debug" : "release"));
      app->appendOutput("");
      app->appendInfo("Type 'help' for a list of commands.");
      app->appendOutput("");
    }

    auto outputCapture =
      std::make_unique<OutputCapture>([&app](const std::string& line) { app->appendLogOutput(line); });

    auto completer = std::make_unique<Completer>();
    completer->setTypeRegistry(&api.getTypes());
    completer->setTuiMode(appMode == App::Mode::tui);
    app->setCompleter(completer.get());

    auto logRouter = std::make_unique<LogRouter>(*app);

    auto engine = std::make_unique<CommandEngine>(config_, api, *app, *logRouter, *completer);
    enginePtr = engine.get();
    engine->getObjectStore().setObjectAddedCallback(
      [&](const std::shared_ptr<Object>& obj)
      {
        completer->onObjectAdded(engine->getScope(), obj);
        engine->onObjectAdded(obj);
      });
    engine->getObjectStore().setObjectRemovedCallback(
      [&](const std::shared_ptr<Object>& obj)
      {
        completer->onObjectRemoved(engine->getScope(), obj);
        engine->onObjectRemoved(obj);
      });

    bool configWatchesRegistered = false;

    auto result = api.execLoop(defaultUpdateFreq,
                               [&]()
                               {
                                 if (app->hasExited() || app->shutdownRequested())
                                 {
                                   if (app->shutdownRequested())
                                   {
                                     // Ctrl+D, exit, or shutdown: request graceful kernel stop.
                                     api.requestKernelStop(0);
                                   }
                                   else
                                   {
                                     // Ctrl+C or Escape: quit immediately.
                                     app->shutdown();
                                     std::_Exit(0);
                                   }
                                   return;
                                 }

                                 app->tick();

                                 outputCapture->drain();
                                 logRouter->update();
                                 engine->update();

                                 if (!configWatchesRegistered)
                                 {
                                   configWatchesRegistered = true;
                                   completer->update(engine->getScope(), engine->getObjectStore(), *logRouter);
                                   if (appMode != App::Mode::repl)
                                   {
                                     for (const auto& watchTarget: config_.watch)
                                     {
                                       engine->execute("watch " + watchTarget);
                                     }
                                   }
                                   for (const auto& listenTarget: config_.listen)
                                   {
                                     engine->execute("listen " + listenTarget);
                                   }
                                 }
                               });

    outputCapture.reset();
    app->shutdown();
    logRouter.reset();

    return result;
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }

private:
  Configuration config_;
};

}  // namespace sen::components::term

SEN_COMPONENT(sen::components::term::TermComponent)
