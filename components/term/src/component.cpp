// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "app.h"
#include "banner.h"
#include "command_engine.h"
#include "completer.h"
#include "log_router.h"
#include "output_capture.h"
#include "theme.h"
#include "util.h"

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

    // Raw, because the app is built first and the engine needs it. The other way round from what this
    // comment used to claim: the engine is declared later, so it is destroyed first. What makes the
    // pointer safe is not lifetime order but reach -- the callback below only runs while the loop runs,
    // and that ends before either unwinds.
    CommandEngine* enginePtr = nullptr;

    auto app = std::make_unique<App>(
      [&enginePtr](const std::string& cmd)
      {
        if (enginePtr != nullptr)
        {
          enginePtr->execute(cmd);
        }
      });

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
    app->setCompleter(completer.get());

    // Create the component's logger before the router's first sweep, so its sink is in place
    // before anything logs through it -- otherwise the first tick's messages reach the real
    // stdout, which FTXUI owns, and corrupt the display.
    getLogger();

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

    auto result = api.execLoop(defaultUpdateFreq,
                               [&]()
                               {
                                 // One exit path for every way of leaving. Ctrl+C used to take
                                 // std::_Exit(0) here, which skipped every other component's
                                 // unload, the transport goodbye and the log flush, and reported
                                 // success whatever had happened.
                                 if (app->hasExited() || app->shutdownRequested())
                                 {
                                   api.requestKernelStop(0);
                                   return;
                                 }

                                 app->tick();

                                 outputCapture->drain();
                                 logRouter->update();
                                 engine->update();
                               });

    outputCapture.reset();
    app->shutdown();

    // The engine goes before the router it holds a reference to. Leaving it to unwind at scope exit
    // left it holding a dangling `LogRouter&` from here to the end of the function -- safe only
    // because `~CommandEngine` happens not to touch it, which is not a property to depend on.
    engine.reset();
    logRouter.reset();

    return result;
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }

private:
  Configuration config_;
};

}  // namespace sen::components::term

SEN_COMPONENT(sen::components::term::TermComponent)
