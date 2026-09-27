// === app.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_APP_H
#define SEN_COMPONENTS_TERM_SRC_APP_H

// component
#include "arg_form.h"
#include "completer.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <chrono>
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
class App;
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
  using CommandHandler = sen::std_util::move_only_function<void(const std::string&)>;

  explicit App(CommandHandler onCommand);
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

  /// Show a short notice over the bottom right of the output pane for about a second. Nothing is
  /// added to the pane, so a notice the user only needs to see once leaves no line behind.
  void showToast(std::string text);

  /// Track a pending method call. Returns an opaque id; pass it to finishPendingCall()
  /// with the result when the call returns.
  [[nodiscard]] std::size_t startPendingCall(std::string description);

  /// Complete a pending call, moving its result into the output pane.
  void finishPendingCall(std::size_t id, ftxui::Element result);

  /// Append plain text to the log pane.
  void appendLogOutput(std::string_view text);

  /// Append a styled element to the output.
  void appendLogElement(ftxui::Element element);

  /// Append a styled element for an event.
  void appendEventElement(ftxui::Element element);

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

  /// The keys the input line handles itself: motion, erasing, history and text.
  bool handleInputKey(const ftxui::Event& event);
  ftxui::Component createRenderer(ftxui::Component wrappedInput);

  ftxui::Component createEventHandler(ftxui::Component renderer);

  // Event sub-handlers called from createEventHandler(). Return true if consumed.
  bool handleFormEvent(ftxui::Event event);
  bool handleCompletionEvent(ftxui::Event event);
  bool handleGlobalEvent(ftxui::Event event);

  /// Handle an event while a paste is being received, and the markers that delimit one.
  bool handlePasteEvent(ftxui::Event event);

  /// Insert pasted text at the cursor, under the same length bound as typing.
  void insertPastedText(std::string_view text);
  bool handleMouseEvent(ftxui::Event event);

  // Copy the current mouse selection to the system clipboard.
  void copySelectionToClipboard();

  /// Copy to the system clipboard and say so in the output. Both copy paths go through here so
  /// the drag-to-select one is not silent.
  void copyToClipboardAndReport(std::string text);

  // Completion rendering helpers.
  [[nodiscard]] ftxui::Element renderCompletionList() const;
  [[nodiscard]] bool toastVisible() const;
  [[nodiscard]] ftxui::Element renderToast() const;
  [[nodiscard]] ftxui::Element renderCompletionHint() const;

private:
  CommandHandler onCommand_;
  std::filesystem::path historyFile_;
  bool exited_ = false;
  bool shutdownRequested_ = false;
  std::size_t nextCallId_ = 0;
  std::size_t activePendingCount_ = 0;
  Completer* completer_ = nullptr;  // not owned

  /// Set between the terminal's paste-start and paste-end markers. Inside a paste every byte is text:
  /// the protocol offers no other way to tell a paste from someone typing very fast, so without this
  /// a newline in pasted text submitted the line and a stray tab opened completion.
  bool pasting_ = false;
  std::size_t pastedNewlines_ = 0;  ///< newlines joined in the paste being received
  unsigned pasteIdleTicks_ = 0;     ///< ticks since the last pasted event, to end a paste whose marker was lost

  /// Bytes of a torn paste marker still owed. FTXUI flushes an incomplete escape after 50 ms and both
  /// markers are six bytes, so a slow link splits one: the prefix is acted on as the whole marker and
  /// this many following bytes are swallowed, instead of landing on the line as text.
  std::size_t markerTailRemaining_ = 0;

  // Render control; the policy is at the redraw decision in tick().
  bool needsRedraw_ = true;
  unsigned idleTickCount_ = 0;
  unsigned pasteModeTicks_ = 0;  ///< ticks since bracketed paste was last re-asserted

  // The transient notice and when it stops being drawn. toastWasVisible_ buys the one extra frame
  // that takes it off the screen once it has expired.
  std::string toastText_;
  std::chrono::steady_clock::time_point toastUntil_;
  bool toastWasVisible_ = false;

  // Last selection auto-copied to the clipboard, used to avoid re-spawning the
  // clipboard helper on mouse releases that don't change the selection.
  std::string lastCopiedSelection_;
  // Set on Left+Released, drained after loop_->RunOnce() completes so FTXUI has finalized the
  // selection.
  bool selectionCheckPending_ = false;

  // No default constructor: built from a factory in init(), which is where the terminal
  // is installed. FTXUI 7 renamed ScreenInteractive to App and kept the old name as an alias.
  std::unique_ptr<ftxui::App> screen_;
  std::unique_ptr<ftxui::Loop> loop_;

  struct UiState;
  std::unique_ptr<UiState> ui_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_APP_H
