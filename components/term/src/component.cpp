// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "app.h"
#include "banner.h"
#include "clipboard.h"
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
#include "sen/core/obj/object.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/term.stl.h"

// std
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>

#ifdef _WIN32
#  include <io.h>

#  include <cstdio>
#else
#  include <unistd.h>
#endif

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr auto* internalSession = "local";

/// term drives a terminal directly: it puts standard input in raw mode and writes the alternate screen
/// to standard output. With either one redirected there is nothing to drive -- FTXUI reads end-of-file
/// from a pipe and returns no event, so the component never exits, while still painting a full frame of
/// escape sequences into whatever standard output is, thirty times a second.
[[nodiscard]] bool standardStreamsAreATerminal()
{
#ifdef _WIN32
  return ::_isatty(::_fileno(stdin)) != 0 && ::_isatty(::_fileno(stdout)) != 0;
#else
  return ::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0;
#endif
}

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
constexpr auto defaultCallTimeout = Duration(std::chrono::seconds(30));

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
    // Refused here and not from init(), and by asking for a stop rather than by returning an error:
    // Runner::terminateIfError turns any error a component reports into dump_backtrace() plus
    // std::terminate(), so a redirected stream would be answered with a crash report.
    //
    // Asked for on every cycle, not once: KernelImpl::requestStop drops a request made before the
    // kernel finishes starting, and this runs inside that window. A single request left the process
    // up with no terminal to draw on and nothing that would ever end it.
    if (!standardStreamsAreATerminal())
    {
      getLogger()->error(
        "term needs a terminal. Standard input and standard output must both be connected to "
        "one, so term cannot run as a service, in a pipeline or in a CI job. Nothing was drawn, "
        "and the kernel is being asked to stop.");
      return api.execLoop(defaultUpdateFreq, [&api]() { api.requestKernelStop(1); });
    }

    // Before the terminal is installed, so it reaches the real console rather than term's own pane.
    // It is the last line of the kernel's startup on screen, and without it the startup log is what
    // the user is left looking at after term exits, with nothing saying it is not the shutdown.
    getLogger()->info("starting term");

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
      app->appendOutput("");  // a blank row above the banner, so it is not flush against the top
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

    bool stopRequested = false;
    auto result = api.execLoop(defaultUpdateFreq,
                               [&]()
                               {
                                 // One exit path for every way of leaving. Ctrl+C used to take
                                 // std::_Exit(0) here, which skipped every other component's
                                 // unload, the transport goodbye and the log flush, and reported
                                 // success whatever had happened.
                                 //
                                 // Asked for once, and then the loop carries on drawing. Returning
                                 // here instead froze the screen for the whole of the kernel's
                                 // shutdown -- term is stopped last, so that is every other
                                 // component's stop and unload -- with the terminal still in raw
                                 // mode, no redraw, and FTXUI's deferred signal handling never
                                 // running, so Ctrl+C and SIGTERM did nothing either.
                                 if ((app->hasExited() || app->shutdownRequested()) && !stopRequested)
                                 {
                                   api.requestKernelStop(0);
                                   stopRequested = true;
                                 }

                                 // The boundary for everything the frame touches. The kernel's
                                 // EXCEPTION_WRAP_BLOCK wraps nothing, and the thread function below it
                                 // has no catch either, so a throw from the renderer, a key handler or
                                 // the discovery drain reached std::terminate and took every other
                                 // component in the process with it. CommandEngine::execute has its own
                                 // try for the same reason; this covers the two paths it cannot see.
                                 //
                                 // Swallowing a frame is the right trade: the user loses one redraw and
                                 // keeps the session, the kernel, and everything else running.
                                 try
                                 {
                                   app->tick();

                                   outputCapture->drain();
                                   logRouter->update();
                                   engine->update();
                                 }
                                 catch (const std::exception& e)
                                 {
                                   app->appendInfo(std::string("Internal error while drawing: ") + e.what());
                                 }
                                 catch (...)
                                 {
                                   app->appendInfo("Internal error while drawing, of an unknown type.");
                                 }
                               });

    // The clipboard worker goes first, and the order is load-bearing on Windows: it can be inside a
    // _popen("clip"), whose child would hold a copy of the capture pipe's write end, and the Windows
    // reader thread is retired by that pipe reaching EOF. A child holding it means no EOF and a join
    // that never returns. This is also the only place that waits for the worker at all.
    clipboard::shutdown();

    outputCapture.reset();

    // One last drain while the screen is still up, so lines that arrived during the final frame are
    // drawn rather than discarded with the queue.
    logRouter->update();

    // The engine goes before the router it holds a reference to. Leaving it to unwind at scope exit
    // left it holding a dangling `LogRouter&` from here to the end of the function -- safe only
    // because `~CommandEngine` happens not to touch it, which is not a property to depend on.
    engine.reset();

    // And the router before the screen. Removing its sink is what gives the console sinks back, so
    // doing it first means the kernel's whole shutdown -- every other component's stop and unload, the
    // transport goodbye, and any error raised while term's own group unloads -- reaches the console.
    // The other order left that phase going nowhere at all: no console sink, and term's queue gone.
    logRouter.reset();

    app->shutdown();

    return result;
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }

private:
  Configuration config_;
};

}  // namespace sen::components::term

SEN_COMPONENT(sen::components::term::TermComponent)
