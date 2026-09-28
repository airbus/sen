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
#include "sen/core/base/result.h"
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

/// term drives a terminal directly: it puts standard input in raw mode and writes the alternate screen to
/// standard output. With either one redirected there is nothing to drive. FTXUI reads end-of-file from a
/// pipe and returns no event, so the component never exits, and it still paints a full frame of escape
/// sequences into whatever standard output is, thirty times a second.
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
    // Returning here stops this component and leaves the rest of the kernel running, which is what
    // lets a configuration carry term and still be started headless. Asking the kernel to stop instead
    // would fail every such run, and reporting an error would be worse still: Runner::terminateIfError
    // turns any error a component reports into dump_backtrace() plus std::terminate(), so a redirected
    // stream would be answered with a crash report.
    if (!standardStreamsAreATerminal())
    {
      getLogger()->error(
        "term needs a terminal. Standard input and standard output must both be connected to one, so "
        "term cannot run as a service, in a pipeline or in a CI job. Nothing is drawn and term is "
        "stopping; the rest of the kernel carries on.");
      return Ok();
    }

    // Before the terminal is installed, so it reaches the real console rather than term's own pane.
    // It is the last line of the kernel's startup on screen, and without it the startup log is what
    // the user is left looking at after term exits, with nothing saying it is not the shutdown.
    getLogger()->info("starting term");

    auto termSource = api.getSource(kernel::BusAddress {internalSession, internalBus});

    // Raw, because the app is built first and the engine needs it. What makes the pointer safe is reach
    // rather than lifetime: the callback below runs only while the loop runs, and that ends before
    // either object unwinds.
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

    // Create term's own logger up front, so `log` lists it from the first frame.
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
                                 // One exit path for every way of leaving, so no route skips another
                                 // component's unload, the transport goodbye or the log flush.
                                 //
                                 // The stop is asked for once and the loop carries on drawing.
                                 // Returning here instead freezes the screen for the whole of the
                                 // kernel's shutdown, which is every other component's stop and
                                 // unload because term is stopped last, with the terminal still in
                                 // raw mode and FTXUI's deferred signal handling never running.
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

    // The clipboard worker goes first, and on Windows the order matters: it can be inside a
    // _popen("clip"), whose child holds a copy of the capture pipe's write end, and the Windows reader
    // thread is retired by that pipe reaching EOF. A child holding it means no EOF and a join that never
    // returns. This is also the only place that waits for the worker.
    clipboard::shutdown();

    outputCapture.reset();

    // One last drain while the screen is still up, so lines that arrived during the final frame are
    // drawn rather than discarded with the queue.
    logRouter->update();

    // The engine goes before the router it holds a reference to. Left to unwind at scope exit it would
    // hold a dangling `LogRouter&` to the end of the function, safe only because `~CommandEngine` happens
    // not to touch it.
    engine.reset();

    // And the router before the screen. Removing its sink is what gives the console sinks back, so doing
    // it first means the rest of the kernel's shutdown reaches the console: every other component's stop
    // and unload, the transport goodbye, and any error raised while term's own group unloads. The other
    // order leaves that phase going nowhere, with no console sink and term's queue already gone.
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
