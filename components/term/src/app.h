// === app.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_APP_H
#define SEN_COMPONENTS_TERM_SRC_APP_H

// local
#include "arg_form.h"
#include "completer.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

// forward declarations
namespace ftxui
{
class ComponentBase;
using Component = std::shared_ptr<ComponentBase>;
class ScreenInteractive;
class Loop;
struct Event;
}  // namespace ftxui

namespace sen::components::term
{

/// The main TUI application. Call `init()` once, then `tick()` each cycle (typically at 30 Hz),
/// and `shutdown()` before the component exits.
class App final
{
  SEN_NOCOPY_NOMOVE(App)

public:
  /// Display modes mirror the DisplayMode enum from the STL definition.
  enum class Mode
  {
    tui,
    repl
  };

  using CommandHandler = sen::std_util::move_only_function<void(const std::string&)>;

  App(CommandHandler onCommand, Mode mode);
  ~App();

  /// Configure the persistent history file. Must be called before init().
  void setHistoryFile(std::filesystem::path path);

  /// Install the terminal (raw mode, alternate screen, mouse tracking) and create the FTXUI
  /// Loop. Call once before the first tick(). No thread is started; the loop is driven
  /// by tick() on the caller's thread.
  void init();

  /// Process all pending terminal events (non-blocking) and draw a frame. Call this from the
  /// Sen component's execLoop callback, typically at 30 Hz.
  void tick();

  /// Uninstall the terminal and tear down the Loop. Call once after the last tick().
  void shutdown();

  /// Append plain text to the command output pane.
  void appendOutput(std::string_view text);

  /// Append a styled info line to the command pane.
  void appendInfo(std::string_view text);

  /// Append a pre-built FTXUI element to the command pane.
  void appendElement(ftxui::Element element);

  /// Track a pending method call. Returns an opaque id; pass it to finishPendingCall()
  /// with the result when the call returns.
  [[nodiscard]] std::size_t startPendingCall(std::string description);

  /// Complete a pending call, moving its result into the output pane.
  void finishPendingCall(std::size_t id, ftxui::Element result);

  /// Get the current number of pending calls.
  [[nodiscard]] std::size_t getPendingCallCount() const;

  /// Append plain text to the log pane.
  void appendLogOutput(std::string_view text);

  /// Append a styled element to the log pane.
  void appendLogElement(ftxui::Element element);

  /// Append a styled element to the events pane.
  void appendEventElement(ftxui::Element element);

  /// Insert-or-update the watch pane entry for `key`.
  void updateWatch(std::string key, ftxui::Element value, bool inlineValue);

  /// Remove a single watch entry by key.
  void removeWatch(const std::string& key);

  /// Drop every watch entry.
  void clearAllWatches();

  /// Mark a watch as connected or disconnected (object removed / returned).
  void setWatchConnected(const std::string& key, bool connected);

  /// Add a pending watch entry for an object that doesn't exist yet. Shown as "(waiting)"
  /// until the object appears and the watch connects.
  void addPendingWatch(const std::string& key);

  /// Make the watch pane visible. Called after registering new watches.
  void showWatchPane();

  /// Set static status bar fields. Called once at startup.
  void setStatusIdentity(std::string_view appName, std::string_view hostName);

  /// Update dynamic status bar fields. Called each cycle.
  void setStatus(std::size_t objectsInScope,
                 std::size_t objectsTotal,
                 bool isRootScope,
                 std::size_t pendingCalls,
                 std::size_t sourceCount,
                 std::size_t listenerCount,
                 std::string_view scopePath,
                 std::string_view timeStr,
                 std::string_view txRate,
                 std::string_view rxRate,
                 double txRateVal,
                 double rxRateVal);

  /// Update the prompt text.
  void setPrompt(std::string_view prompt);

  /// Set the completer for Tab completion (must outlive the App).
  void setCompleter(Completer* completer);

  /// Clear the command output pane.
  void clearCommandPane();

  /// Returns true if the user has requested exit (Escape / Ctrl+D / exit command).
  [[nodiscard]] bool hasExited() const noexcept;
  [[nodiscard]] bool shutdownRequested() const noexcept;
  void requestShutdown() noexcept;

  /// Returns the display mode (tui or repl).
  [[nodiscard]] Mode displayMode() const noexcept { return mode_; }

  /// Mark the UI as dirty so the next tick() will redraw. Callers rarely need
  /// this directly; every App mutation already triggers it.
  void requestRedraw() noexcept { needsRedraw_ = true; }

  /// Open a guided-input form for a method call.
  void openArgForm(ArgForm form);

  /// Whether the input area is currently occupied by a form.
  [[nodiscard]] bool hasActiveForm() const noexcept;

private:
  // init() helpers: each builds one layer of the FTXUI component tree.
  ftxui::Component createInputComponent();
  ftxui::Component createRenderer(ftxui::Component wrappedInput);
  ftxui::Component createEventHandler(ftxui::Component renderer);

  // Event sub-handlers called from createEventHandler(). Return true if consumed.
  bool handleFormEvent(ftxui::Event event);
  bool handleCompletionEvent(ftxui::Event event);
  bool handleGlobalEvent(ftxui::Event event);
  bool handleMouseEvent(ftxui::Event event);

  // Copy the current mouse selection to the system clipboard.
  void copySelectionToClipboard();

  // Completion rendering helpers (used by both TUI and REPL renderers).
  ftxui::Element renderCompletionList() const;
  ftxui::Element renderCompletionHint() const;

private:
  CommandHandler onCommand_;
  Mode mode_;
  std::filesystem::path historyFile_;
  bool exited_ = false;
  bool shutdownRequested_ = false;
  bool logPaused_ = false;
  std::size_t nextCallId_ = 0;
  std::size_t activePendingCount_ = 0;
  Completer* completer_ = nullptr;  // not owned

  // Render control. tick() only posts a Custom event (which forces FTXUI to
  // re-render) when needsRedraw_ is true, when a pending call is animating a
  // spinner, or when the idle safety-net counter fires. Every App mutation
  // setter flips needsRedraw_ on so async updates (log messages, watch
  // changes, discovery notifications) still reach the screen immediately.
  bool needsRedraw_ = true;
  unsigned idleTickCount_ = 0;

  // Last selection auto-copied to the clipboard, used to avoid re-spawning the
  // clipboard helper on mouse releases that don't change the selection.
  std::string lastCopiedSelection_;
  // Set on Left+Released (when not resizing a pane separator); drained after
  // loop_->RunOnce() completes so FTXUI has finalized the selection.
  bool selectionCheckPending_ = false;

  // ScreenInteractive is not moveable and needs Fullscreen(); heap-allocated in init().
  std::unique_ptr<ftxui::ScreenInteractive> screen_;
  std::unique_ptr<ftxui::Loop> loop_;

  struct UiState;
  std::unique_ptr<UiState> ui_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_APP_H
