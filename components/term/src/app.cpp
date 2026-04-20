// === app.cpp =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "app.h"

// local
#include "app_renderers.h"
#include "clipboard.h"
#include "input_pane.h"
#include "output_pane.h"
#include "status_bar.h"
#include "styles.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"

// ftxui
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

// std
#include <cstddef>
#include <map>
#include <string>
#include <utility>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::size_t commandPaneMaxLines = 5000U;
constexpr std::size_t logPaneMaxLines = 1000U;
constexpr int pageScrollLines = 10;
constexpr int mouseWheelScrollLines = 3;
constexpr int completionMinWidth = 40;
constexpr int completionMaxRows = 10;
constexpr int completionColPadding = 3;

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// UiState
//--------------------------------------------------------------------------------------------------------------

struct App::UiState
{
  OutputPane commandPane;
  OutputPane logPane;
  OutputPane eventPane;
  InputPane inputPane;
  StatusBarData statusBar;

  // When set, the input area is replaced by the form. Consumed by the render pass and the event
  // handler; cleared on submit or cancel.
  std::optional<ArgForm> activeForm;

  // Bottom pane state. Hidden by default; toggle with F4.
  bool logPaneVisible = false;
  enum class BottomTab
  {
    logs,
    events
  };
  BottomTab bottomTab = BottomTab::logs;
  std::size_t unreadLogCount = 0;
  std::size_t unreadEventCount = 0;
  int logPaneHeight = 0;  // 0 = use default (1/3 of terminal height), set on first render
  bool logDragging = false;
  enum class ActivePane
  {
    command,
    log
  };
  ActivePane activePane = ActivePane::command;

  // Watch pane state. Each entry's "age" decrements per frame for a flash-on-change effect.
  std::map<std::string, WatchEntry> watches;
  bool watchPaneVisible = false;
  int watchScrollRow = 0;      // which row in the watch pane's vbox to focus for yframe scrolling
  int watchPaneWidth = 0;      // 0 = use default (1/3 of terminal); nonzero = user-dragged size
  bool watchDragging = false;  // true while the user is dragging the separator

  // Completion state
  std::vector<Completion> completionCandidates;
  int completionCycleIndex = -1;   // -1 = not cycling, 0+ = current candidate
  int completionReplaceFrom = 0;   // start of the token being replaced
  std::string completionOriginal;  // the original token before cycling started
  // Completion accept hint (description + action). The action is always shown; description is
  // truncated on narrow terminals.
  std::string completionHint;
  std::string completionHintAction;

  // Quit-confirmation state. ESC on a non-empty input discards the buffer (k9s-style
  // "back / cancel"); ESC on an empty input arms this flag, which displays a confirmation
  // row under the prompt; a second ESC then shuts the app down. Any other key dismisses
  // the armed state (no timeout: the confirmation sticks until the user resolves it).
  bool exitConfirmPending = false;

  void clearCompletion()
  {
    completionCandidates.clear();
    completionCycleIndex = -1;
    completionOriginal.clear();
    completionHint.clear();
    completionHintAction.clear();
  }

  int cursorPos = 0;

  UiState(InputPane::SubmitCallback onSubmit, const std::filesystem::path& historyFile): inputPane(std::move(onSubmit))
  {
    if (!historyFile.empty())
    {
      inputPane.setHistoryFile(historyFile);
      inputPane.loadHistory();
    }
  }
};

//--------------------------------------------------------------------------------------------------------------
// App
//--------------------------------------------------------------------------------------------------------------

App::App(CommandHandler onCommand, Mode mode): onCommand_(std::move(onCommand)), mode_(mode) {}

App::~App() { shutdown(); }

void App::setHistoryFile(std::filesystem::path path) { historyFile_ = std::move(path); }

void App::openArgForm(ArgForm form)
{
  ui_->activeForm = std::move(form);
  requestRedraw();
}

bool App::hasActiveForm() const noexcept { return ui_ && ui_->activeForm.has_value(); }

void App::appendOutput(std::string_view text)
{
  ui_->commandPane.appendText(text);
  requestRedraw();
}

void App::appendInfo(std::string_view text)
{
  ui_->commandPane.appendInfo(text);
  requestRedraw();
}

void App::appendElement(ftxui::Element element)
{
  ui_->commandPane.appendElement(std::move(element));
  requestRedraw();
}

void App::updateWatch(std::string key, ftxui::Element value, bool inlineValue)
{
  auto& entry = ui_->watches[key];
  entry.value = std::move(value);
  entry.inlineValue = inlineValue;
  entry.connected = true;
  entry.pending = false;
  entry.age = watchMaxAge;
  requestRedraw();
}

void App::removeWatch(const std::string& key)
{
  ui_->watches.erase(key);
  requestRedraw();
}

void App::clearAllWatches()
{
  ui_->watches.clear();
  requestRedraw();
}

void App::setWatchConnected(const std::string& key, bool connected)
{
  auto it = ui_->watches.find(key);
  if (it != ui_->watches.end())
  {
    it->second.connected = connected;
    if (connected)
    {
      it->second.pending = false;
    }
    requestRedraw();
  }
}

void App::addPendingWatch(const std::string& key)
{
  auto& entry = ui_->watches[key];
  entry.value = ftxui::text("(waiting)") | styles::mutedText();
  entry.inlineValue = true;
  entry.connected = false;
  entry.pending = true;
  entry.age = 0;
  requestRedraw();
}

void App::showWatchPane()
{
  if (mode_ != Mode::repl)
  {
    ui_->watchPaneVisible = true;
    requestRedraw();
  }
}

std::size_t App::startPendingCall(std::string description)
{
  auto id = ++nextCallId_;
  ui_->commandPane.appendPendingCall(id, std::move(description));
  ++activePendingCount_;
  requestRedraw();
  return id;
}

void App::finishPendingCall(std::size_t id, ftxui::Element result)
{
  ui_->commandPane.replacePendingCall(id, std::move(result));
  if (activePendingCount_ > 0)
  {
    --activePendingCount_;
  }
  requestRedraw();
}

std::size_t App::getPendingCallCount() const { return activePendingCount_; }

void App::appendLogOutput(std::string_view text)
{
  if (!logPaused_)
  {
    ui_->logPane.appendText(text);
  }
  if (!ui_->logPaneVisible)
  {
    ++ui_->unreadLogCount;
  }
  requestRedraw();
}

void App::appendLogElement(ftxui::Element element)
{
  if (!logPaused_)
  {
    ui_->logPane.appendElement(std::move(element));
  }
  if (!ui_->logPaneVisible)
  {
    ++ui_->unreadLogCount;
  }
  requestRedraw();
}

void App::appendEventElement(ftxui::Element element)
{
  if (mode_ == Mode::repl)
  {
    // In REPL mode, events flow inline with command output.
    ui_->commandPane.appendElement(std::move(element));
    requestRedraw();
    return;
  }
  ui_->eventPane.appendElement(std::move(element));
  if (!ui_->logPaneVisible || ui_->bottomTab != UiState::BottomTab::events)
  {
    ++ui_->unreadEventCount;
  }
  requestRedraw();
}

void App::clearCommandPane()
{
  ui_->commandPane.clear();
  requestRedraw();
}

void App::setCompleter(Completer* completer) { completer_ = completer; }

void App::setStatusIdentity(std::string_view appName, std::string_view hostName)
{
  ui_->statusBar.appName = appName;
  ui_->statusBar.hostName = hostName;
  requestRedraw();
}

void App::setStatus(std::size_t objectsInScope,
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
                    double rxRateVal)
{
  ui_->statusBar.objectsInScope = objectsInScope;
  ui_->statusBar.objectsTotal = objectsTotal;
  ui_->statusBar.isRootScope = isRootScope;
  ui_->statusBar.pendingCalls = pendingCalls;
  ui_->statusBar.sourceCount = sourceCount;
  ui_->statusBar.listenerCount = listenerCount;
  ui_->statusBar.scopePath = scopePath;
  ui_->statusBar.timeStr = timeStr;
  ui_->statusBar.txRateStr = txRate;
  ui_->statusBar.txRate = txRateVal;
  ui_->statusBar.rxRate = rxRateVal;
  ui_->statusBar.rxRateStr = rxRate;
  requestRedraw();
}

void App::setPrompt(std::string_view prompt)
{
  ui_->inputPane.setPrompt(prompt);
  requestRedraw();
}

bool App::hasExited() const noexcept { return exited_; }
bool App::shutdownRequested() const noexcept { return shutdownRequested_; }
void App::requestShutdown() noexcept { shutdownRequested_ = true; }

//--------------------------------------------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------------------------------------------

void App::init()
{
  // `ScreenInteractive` is not movable, so `make_unique` can't forward the
  // `Fullscreen()` factory's return value. A bare `new` into `reset()` is the
  // only way to heap-allocate it.
  screen_.reset(new ftxui::ScreenInteractive(ftxui::ScreenInteractive::Fullscreen()));  // NOLINT(modernize-make-unique)

  ui_ = std::make_unique<UiState>(
    [this](const std::string& command)
    {
      if (onCommand_)
      {
        onCommand_(command);
      }
    },
    historyFile_);

  ui_->commandPane.setMaxLines(commandPaneMaxLines);
  ui_->logPane.setMaxLines(logPaneMaxLines);
  ui_->eventPane.setMaxLines(logPaneMaxLines);

  if (mode_ == Mode::repl)
  {
    ui_->commandPane.setBottomAligned(true);
  }

  auto input = createInputComponent();
  auto renderer = createRenderer(input);
  auto eventHandler = createEventHandler(renderer);
  loop_ = std::make_unique<ftxui::Loop>(screen_.get(), eventHandler);
}

//--------------------------------------------------------------------------------------------------------------
// Completion rendering
//--------------------------------------------------------------------------------------------------------------

ftxui::Element App::renderCompletionList() const
{
  auto total = ui_->completionCandidates.size();
  auto cycleIdx = ui_->completionCycleIndex;

  std::size_t maxTextWidth = 0;
  for (const auto& c: ui_->completionCandidates)
  {
    maxTextWidth = std::max(maxTextWidth, c.text.size());
  }
  int colWidth = checkedConversion<int>(maxTextWidth) + completionColPadding;

  auto termSize = ftxui::Terminal::Size();
  int availWidth = std::max(completionMinWidth, termSize.dimx - 2);
  int numCols = std::max(1, availWidth / colWidth);
  int numRows = checkedConversion<int>((total + checkedConversion<std::size_t>(numCols) - 1) /
                                       checkedConversion<std::size_t>(numCols));

  int scrollOffset = 0;
  if (numRows > completionMaxRows && cycleIdx >= 0)
  {
    int selectedRow = cycleIdx % numRows;
    if (selectedRow >= completionMaxRows)
    {
      scrollOffset = selectedRow - completionMaxRows + 1;
    }
  }

  int visibleRows = std::min(numRows, completionMaxRows);

  ftxui::Elements rows;
  for (int row = scrollOffset; row < scrollOffset + visibleRows; ++row)
  {
    ftxui::Elements cells;
    for (int col = 0; col < numCols; ++col)
    {
      auto idx = checkedConversion<std::size_t>(col) * checkedConversion<std::size_t>(numRows) +
                 checkedConversion<std::size_t>(row);
      if (idx >= total)
      {
        break;
      }
      const auto& c = ui_->completionCandidates[idx];
      bool selected = (checkedConversion<int>(idx) == cycleIdx);
      auto lastDot = c.text.rfind('.');
      auto label = (lastDot != std::string::npos) ? c.text.substr(lastDot + 1) : c.text;
      auto entry = ftxui::text(label);
      if (selected)
      {
        entry = entry | ftxui::inverted;
      }
      else
      {
        entry = entry | styles::completionStyle(c.kind);
      }
      cells.push_back(ftxui::hbox({ftxui::text("  "), entry}) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, colWidth));
    }
    rows.push_back(ftxui::hbox(std::move(cells)));
  }

  {
    std::string info = "  ";
    if (cycleIdx >= 0)
    {
      info += std::to_string(cycleIdx + 1) + "/" + std::to_string(total);
      const auto& sel = ui_->completionCandidates[checkedConversion<std::size_t>(cycleIdx)];
      if (!sel.detail.empty())
      {
        info += "  " + sel.detail;
      }
      else if (!sel.display.empty())
      {
        info += "  " + sel.display;
      }
    }
    else if (numRows > completionMaxRows)
    {
      info += std::to_string(total) + " matches (Tab to cycle)";
    }
    rows.push_back(ftxui::text(info) | styles::mutedText() | styles::inputFg());
  }

  return ftxui::vbox(std::move(rows));
}

ftxui::Element App::renderCompletionHint() const
{
  // The action word (run/args) is rendered inline next to the cursor by the input
  // renderer, so this hint line only carries the description.
  const std::string& description = ui_->completionHint;
  const std::string leftPad = "  ";

  const auto termSize = ftxui::Terminal::Size();
  constexpr int rightMargin = 2;
  const int available = termSize.dimx - checkedConversion<int>(leftPad.size()) - rightMargin;

  std::string displayDescription = description;
  if (checkedConversion<int>(displayDescription.size()) > available)
  {
    if (available >= 4)
    {
      displayDescription =
        displayDescription.substr(0, checkedConversion<std::size_t>(available - 3)) + unicode::ellipsis;
    }
    else
    {
      displayDescription.clear();
    }
  }

  return ftxui::hbox({ftxui::text(leftPad), ftxui::text(displayDescription) | styles::hintText()});
}

//--------------------------------------------------------------------------------------------------------------
// init() helpers
//--------------------------------------------------------------------------------------------------------------

ftxui::Component App::createInputComponent()
{
  return ftxui::CatchEvent(
    ftxui::Renderer(
      [this](bool focused) -> ftxui::Element
      {
        auto& buf = ui_->inputPane.getBuffer();
        const auto& promptStr = ui_->inputPane.getPrompt();
        auto promptPathDeco = styles::promptPath();
        auto promptSymbolDeco = styles::promptSymbol();

        ui_->cursorPos = std::min(ui_->cursorPos, checkedConversion<int>(buf.size()));

        // Available width for soft-wrapping: first line subtracts the prompt width.
        auto termSize = ftxui::Terminal::Size();
        int termWidth = std::max(20, termSize.dimx);
        int promptWidth = checkedConversion<int>(promptStr.size());
        int firstLineChars = std::max(1, termWidth - promptWidth);
        int contLineChars = std::max(1, termWidth);

        // Split the buffer into visual lines, tracking the buffer range each covers.
        struct VisualLine
        {
          int bufStart;  // index into buf where this visual line's buffer content starts
          int bufEnd;    // one past end
          bool isFirst;  // first visual line (has the prompt)
        };

        std::vector<VisualLine> vlines;
        int pos = 0;
        int remaining = checkedConversion<int>(buf.size());

        // First visual line
        int firstLen = std::min(remaining, firstLineChars);
        vlines.push_back({0, firstLen, true});
        pos = firstLen;
        remaining -= firstLen;

        // Continuation lines
        while (remaining > 0)
        {
          int lineLen = std::min(remaining, contLineChars);
          vlines.push_back({pos, pos + lineLen, false});
          pos += lineLen;
          remaining -= lineLen;
        }

        // If the buffer is empty we still have the first line.
        if (vlines.empty())
        {
          vlines.push_back({0, 0, true});
        }

        // Find which visual line contains the cursor.
        int cursorLine = 0;
        int cursorCol = ui_->cursorPos;  // column within the visual line's buffer segment
        for (int i = 0; i < checkedConversion<int>(vlines.size()); ++i)
        {
          const auto& vl = vlines[checkedConversion<std::size_t>(i)];
          // The cursor sits on this line if it falls within [bufStart, bufEnd).
          // Special case: cursor == bufEnd is on this line only if it's the last line
          // (the blinking cursor after the last character).
          if (ui_->cursorPos >= vl.bufStart &&
              (ui_->cursorPos < vl.bufEnd || i == checkedConversion<int>(vlines.size()) - 1))
          {
            cursorLine = i;
            cursorCol = ui_->cursorPos - vl.bufStart;
            break;
          }
        }

        // Render each visual line.
        ftxui::Elements rows;
        for (int i = 0; i < checkedConversion<int>(vlines.size()); ++i)
        {
          const auto& vl = vlines[checkedConversion<std::size_t>(i)];
          auto segment = buf.substr(checkedConversion<std::size_t>(vl.bufStart),
                                    checkedConversion<std::size_t>(vl.bufEnd - vl.bufStart));

          ftxui::Elements parts;
          if (vl.isFirst)
          {
            auto symPos = promptStr.find(unicode::promptSymbol);
            auto pathEnd = symPos != std::string::npos ? symPos : promptStr.size();
            std::string pathPart(promptStr.substr(0, pathEnd));

            parts.push_back(ftxui::text(pathPart) | promptPathDeco);

            if (symPos != std::string::npos)
            {
              parts.push_back(ftxui::text(std::string(promptStr.substr(symPos, 3))) | promptSymbolDeco);
              parts.push_back(ftxui::text(std::string(promptStr.substr(symPos + 3))));
            }
          }

          if (focused && i == cursorLine)
          {
            auto before = segment.substr(0, checkedConversion<std::size_t>(cursorCol));
            auto atCursor = (cursorCol < checkedConversion<int>(segment.size()))
                              ? segment.substr(checkedConversion<std::size_t>(cursorCol), 1)
                              : std::string(" ");
            auto after = (cursorCol < checkedConversion<int>(segment.size()))
                           ? segment.substr(checkedConversion<std::size_t>(cursorCol) + 1)
                           : std::string();
            parts.push_back(ftxui::text(before));
            parts.push_back(ftxui::text(atCursor) | ftxui::inverted | ftxui::focus);
            parts.push_back(ftxui::text(after));
          }
          else
          {
            parts.push_back(ftxui::text(segment));
          }

          // Inline ghost hint rendered after the cursor on the last visual line, only when
          // there's enough room. Priority order:
          //   1. tab-completion action ("args" / "run" with a return marker): user has a
          //      pending choice.
          //   2. grammar hint (e.g. "`FROM`"): the parser's expected next token.
          // Error messages are intentionally not in this chain: they are typically too
          // long to fit after the query and get their own row below the input (see the
          // dedicated error row appended after this loop).
          bool isLastLine = (i == checkedConversion<int>(vlines.size()) - 1);
          if (isLastLine && focused && !ui_->completionHintAction.empty())
          {
            int promptCells = vl.isFirst ? promptWidth : 0;
            int segCells = vl.bufEnd - vl.bufStart;
            int cursorExtraCell = (cursorLine == i && cursorCol >= segCells) ? 1 : 0;
            int usedCells = promptCells + segCells + cursorExtraCell;

            // "  " + ⏎ (1 cell) + action word (ASCII).
            constexpr int ghostPrefixCells = 3;
            int ghostCells = ghostPrefixCells + checkedConversion<int>(ui_->completionHintAction.size());

            if (usedCells + ghostCells < termWidth)
            {
              // Two leading spaces, then the return-symbol glyph (3 bytes, 1 cell), then
              // the action word (ASCII).
              std::string ghost = std::string("  ") + unicode::returnSymbol + ui_->completionHintAction;
              parts.push_back(ftxui::text(ghost) | styles::mutedText());
            }
          }

          rows.push_back(ftxui::hbox(std::move(parts)));
        }

        // Quit confirmation row: one ESC on an empty buffer arms this; a second ESC
        // shuts down; any other key cancels.
        if (ui_->exitConfirmPending)
        {
          rows.push_back(
            ftxui::hbox({ftxui::text("  "),
                         ftxui::text("Press ESC again to exit · any other key to cancel") | styles::mutedText()}));
        }

        return ftxui::vbox(std::move(rows));
      }),
    [this](const ftxui::Event& event) -> bool
    {
      auto& buf = ui_->inputPane.getBuffer();

      if (event == ftxui::Event::Return)
      {
        ui_->cursorPos = 0;
        ui_->commandPane.scrollToBottom();
        ui_->inputPane.submit();
        return true;
      }
      if (event == ftxui::Event::ArrowUp)
      {
        ui_->inputPane.historyUp();
        ui_->cursorPos = checkedConversion<int>(buf.size());
        return true;
      }
      if (event == ftxui::Event::ArrowDown)
      {
        ui_->inputPane.historyDown();
        ui_->cursorPos = checkedConversion<int>(buf.size());
        return true;
      }
      if (event == ftxui::Event::ArrowLeft)
      {
        if (ui_->cursorPos > 0)
        {
          --ui_->cursorPos;
        }
        return true;
      }
      if (event == ftxui::Event::ArrowRight)
      {
        if (ui_->cursorPos < checkedConversion<int>(buf.size()))
        {
          ++ui_->cursorPos;
        }
        return true;
      }
      if (event == ftxui::Event::Home)
      {
        ui_->cursorPos = 0;
        return true;
      }
      if (event == ftxui::Event::End)
      {
        ui_->cursorPos = checkedConversion<int>(buf.size());
        return true;
      }
      if (event == ftxui::Event::Backspace)
      {
        if (ui_->cursorPos > 0 && !buf.empty())
        {
          buf.erase(checkedConversion<std::size_t>(ui_->cursorPos) - 1, 1);
          --ui_->cursorPos;
        }
        return true;
      }
      if (event == ftxui::Event::Delete)
      {
        if (ui_->cursorPos < checkedConversion<int>(buf.size()))
        {
          buf.erase(checkedConversion<std::size_t>(ui_->cursorPos), 1);
        }
        return true;
      }
      if (event.is_character())
      {
        buf.insert(checkedConversion<std::size_t>(ui_->cursorPos), event.character());
        ui_->cursorPos += checkedConversion<int>(event.character().size());
        return true;
      }
      return false;
    });
}

ftxui::Component App::createRenderer(ftxui::Component wrappedInput)
{
  return ftxui::Renderer(
    wrappedInput,
    [this, wrappedInput]() -> ftxui::Element
    {
      // REPL mode: minimal layout with no panes or status bar.
      if (mode_ == Mode::repl)
      {
        auto commandArea = ui_->commandPane.render() | styles::inputBg() | styles::inputFg();
        auto inputArea = wrappedInput->Render() | styles::inputBg() | styles::inputFg();

        ftxui::Element completionArea = ftxui::emptyElement();
        if (!ui_->completionCandidates.empty())
        {
          completionArea = renderCompletionList();
        }
        else if (!ui_->completionHint.empty() || !ui_->completionHintAction.empty())
        {
          completionArea = renderCompletionHint();
        }

        if (ui_->activeForm.has_value())
        {
          auto formArea = renderArgForm(*ui_->activeForm);
          return ftxui::vbox({commandArea, std::move(formArea)});
        }

        return ftxui::vbox({commandArea, completionArea | styles::completionBg() | styles::inputFg(), inputArea});
      }

      // TUI mode: full layout with panes, status bar, and watches.
      ui_->statusBar.unreadLogCount = ui_->unreadLogCount;
      ui_->statusBar.unreadEventCount = ui_->unreadEventCount;
      ui_->statusBar.logPaneVisible = ui_->logPaneVisible;
      ui_->statusBar.watchCount = ui_->watches.size();
      ui_->statusBar.watchPaneVisible = ui_->watchPaneVisible;
      ui_->statusBar.pendingCalls = activePendingCount_;
      ui_->statusBar.modeHint =
        ui_->activeForm.has_value() ? std::optional<std::string>(formModeHint(*ui_->activeForm)) : std::nullopt;

      bool anyYoung = false;
      for (auto& [key, entry]: ui_->watches)
      {
        if (entry.age > 0U)
        {
          --entry.age;
          anyYoung = true;
        }
      }
      if (anyYoung)
      {
        screen_->RequestAnimationFrame();
      }

      auto commandArea = ui_->commandPane.render() | styles::inputBg() | styles::inputFg();
      auto statusBar = renderStatusBar(ui_->statusBar);
      auto inputArea = wrappedInput->Render() | styles::inputBg() | styles::inputFg();

      ftxui::Element completionArea = ftxui::emptyElement();
      if (!ui_->completionCandidates.empty())
      {
        completionArea = renderCompletionList();
      }
      else if (!ui_->completionHint.empty())
      {
        completionArea = renderCompletionHint();
      }

      ftxui::Elements sections;

      if (ui_->logPaneVisible || ui_->watchPaneVisible)
      {
        auto cmdTitle = ftxui::hbox({
                          ftxui::text(std::string(" ") + unicode::paneCommands + " Commands") | ftxui::bold,
                          ftxui::filler(),
                        }) |
                        styles::paneTitleBg() | styles::paneFg();
        commandArea = ftxui::vbox({cmdTitle, commandArea | ftxui::flex}) | ftxui::flex;
      }

      if (ui_->logPaneVisible)
      {
        bool isLogsTab = (ui_->bottomTab == UiState::BottomTab::logs);

        auto logsTab = ftxui::hbox({
          ftxui::text(std::string(" ") + unicode::paneLogs + " Logs") | (isLogsTab ? ftxui::bold : styles::mutedText()),
          ftxui::text(" F3 ") | styles::mutedText(),
        });
        auto eventsTab = ftxui::hbox({
          ftxui::text(std::string(" ") + unicode::paneEvents + " Events") |
            (isLogsTab ? styles::mutedText() : ftxui::bold),
          ftxui::text(" F4 ") | styles::mutedText(),
        });

        auto title = ftxui::hbox({
                       logsTab,
                       ftxui::text("|") | styles::mutedText(),
                       eventsTab,
                       ftxui::filler(),
                       ftxui::text("F2 clear ") | styles::mutedText(),
                     }) |
                     styles::paneTitleBg() | styles::paneFg();

        auto termSize = ftxui::Terminal::Size();
        if (ui_->logPaneHeight <= 0)
        {
          ui_->logPaneHeight = termSize.dimy / 3;
        }
        ui_->logPaneHeight = std::max(3, std::min(ui_->logPaneHeight, termSize.dimy - 10));

        auto& activeBottomPane = isLogsTab ? ui_->logPane : ui_->eventPane;
        auto paneArea = activeBottomPane.render() | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, ui_->logPaneHeight) |
                        styles::paneBg() | styles::paneFg();
        sections.push_back(ftxui::vbox({title, paneArea}));
      }

      ftxui::Element mainRow;
      if (ui_->watchPaneVisible)
      {
        auto watchRows = renderWatchEntries(ui_->watches);

        ui_->watchScrollRow = std::max(0, std::min(ui_->watchScrollRow, checkedConversion<int>(watchRows.size()) - 1));
        if (!watchRows.empty())
        {
          watchRows[checkedConversion<std::size_t>(ui_->watchScrollRow)] =
            watchRows[checkedConversion<std::size_t>(ui_->watchScrollRow)] | ftxui::focus;
        }

        auto watchArea = ftxui::vbox(std::move(watchRows)) | ftxui::vscroll_indicator | ftxui::yframe | ftxui::flex |
                         styles::paneBg() | styles::paneFg();
        auto watchTitle = ftxui::hbox({
                            ftxui::text(std::string(" ") + unicode::paneWatches + " Watches") | ftxui::bold,
                            ftxui::text(" F5") | styles::mutedText(),
                            ftxui::filler(),
                          }) |
                          styles::paneTitleBg() | styles::paneFg();
        auto watchPanel = ftxui::vbox({watchTitle, watchArea});

        auto termSize = ftxui::Terminal::Size();
        int watchWidth = (ui_->watchPaneWidth > 0) ? ui_->watchPaneWidth : std::max(30, termSize.dimx / 3);
        watchWidth = std::max(15, std::min(watchWidth, termSize.dimx - 30));

        mainRow = ftxui::hbox({commandArea | ftxui::flex,
                               ftxui::text(" ") | styles::paneTitleBg(),
                               watchPanel | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, watchWidth)});
      }
      else
      {
        mainRow = commandArea;
      }

      sections.push_back(mainRow | ftxui::flex);
      ftxui::Element outputSection = ftxui::vbox(std::move(sections));

      if (ui_->activeForm.has_value())
      {
        auto formArea = renderArgForm(*ui_->activeForm);
        return ftxui::vbox({outputSection | ftxui::flex, statusBar, std::move(formArea)}) | ftxui::flex;
      }

      return ftxui::vbox({outputSection | ftxui::flex,
                          statusBar,
                          completionArea | styles::completionBg() | styles::inputFg(),
                          inputArea}) |
             ftxui::flex;
    });
}

ftxui::Component App::createEventHandler(ftxui::Component renderer)
{
  return ftxui::CatchEvent(renderer,
                           [this](ftxui::Event event) -> bool
                           {
                             if (ui_->activeForm.has_value())
                             {
                               return handleFormEvent(event);
                             }
                             if (handleCompletionEvent(event))
                             {
                               return true;
                             }
                             if (handleGlobalEvent(event))
                             {
                               return true;
                             }
                             if (handleMouseEvent(event))
                             {
                               return true;
                             }
                             return false;
                           });
}

//--------------------------------------------------------------------------------------------------------------
// Event sub-handlers
//--------------------------------------------------------------------------------------------------------------

bool App::handleFormEvent(ftxui::Event event)
{
  auto& form = *ui_->activeForm;

  if (event == ftxui::Event::Escape)
  {
    ui_->activeForm.reset();
    return true;
  }
  if (event == ftxui::Event::Tab || event == ftxui::Event::ArrowDown)
  {
    form.focusNext();
    return true;
  }
  if (event == ftxui::Event::TabReverse || event == ftxui::Event::ArrowUp)
  {
    form.focusPrev();
    return true;
  }
  if (event == ftxui::Event::Backspace)
  {
    form.backspace();
    return true;
  }
  if (event == ftxui::Event::Special({14}))  // Ctrl+N
  {
    form.addElementToFocusedSequence();
    return true;
  }
  if (event == ftxui::Event::Special({24}))  // Ctrl+X
  {
    form.removeFocusedSequenceElement();
    return true;
  }
  if (event == ftxui::Event::Special({15}))  // Ctrl+O
  {
    form.toggleFocusedOptional();
    return true;
  }
  if (event == ftxui::Event::ArrowLeft)
  {
    form.cycleFocused(false);
    return true;
  }
  if (event == ftxui::Event::ArrowRight)
  {
    form.cycleFocused(true);
    return true;
  }
  if (event == ftxui::Event::Character(" "))
  {
    if (form.focusedEditor() == EditorKind::boolean)
    {
      form.toggleFocused();
      return true;
    }
  }
  if (event == ftxui::Event::Return)
  {
    auto result = form.trySubmit();
    if (result.isOk())
    {
      auto inline_ =
        formatInlineInvocation(form.objectName(), form.methodName(), form.method().getArgs(), result.getValue());
      ui_->activeForm.reset();
      if (onCommand_)
      {
        onCommand_(inline_);
      }
    }
    else
    {
      auto failIdx = result.getError().fieldIndex;
      while (form.focusedIndex() != failIdx)
      {
        form.focusNext();
      }
    }
    return true;
  }
  if (event.is_character())
  {
    form.insertText(event.character());
    return true;
  }
  return false;  // let other handlers (scroll etc.) have the event
}

bool App::handleCompletionEvent(ftxui::Event event)
{
  if (ui_->completionCycleIndex >= 0 && (event == ftxui::Event::Return || event == ftxui::Event::Character(".")))
  {
    auto& buf = ui_->inputPane.getBuffer();
    const auto& sel = ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)];
    bool dotPressed = event == ftxui::Event::Character(".");
    bool needsContinuation = dotPressed && (sel.kind == CompletionKind::path || sel.kind == CompletionKind::object);

    if (needsContinuation)
    {
      auto from = checkedConversion<std::size_t>(ui_->completionReplaceFrom);
      buf.replace(from, buf.size() - from, sel.text + ".");
      ui_->cursorPos = ui_->completionReplaceFrom + checkedConversion<int>(sel.text.size()) + 1;
      ui_->clearCompletion();

      auto nextResult = completer_->complete(buf, ui_->cursorPos);
      if (!nextResult.candidates.empty())
      {
        ui_->completionReplaceFrom = nextResult.replaceFrom;
        ui_->completionOriginal =
          buf.substr(checkedConversion<std::size_t>(nextResult.replaceFrom),
                     checkedConversion<std::size_t>(nextResult.replaceTo - nextResult.replaceFrom));
        ui_->completionCandidates = std::move(nextResult.candidates);
        ui_->completionCycleIndex = -1;
      }
    }
    else
    {
      std::string description = !sel.detail.empty() ? sel.detail : sel.display;
      std::string action;
      if (sel.kind == CompletionKind::method)
      {
        action = (sel.argCount == 0U) ? "run" : "args";
      }
      auto from = checkedConversion<std::size_t>(ui_->completionReplaceFrom);
      buf.replace(from, buf.size() - from, sel.text + " ");
      ui_->cursorPos = ui_->completionReplaceFrom + checkedConversion<int>(sel.text.size()) + 1;
      ui_->clearCompletion();
      ui_->completionHint = std::move(description);
      ui_->completionHintAction = std::move(action);
    }
    return true;
  }

  if ((!ui_->completionCandidates.empty() || !ui_->completionHint.empty() || !ui_->completionHintAction.empty()) &&
      (event.is_character() || event == ftxui::Event::Return || event == ftxui::Event::Backspace ||
       event == ftxui::Event::Delete))
  {
    ui_->clearCompletion();
  }

  if (event == ftxui::Event::Escape && !ui_->completionCandidates.empty())
  {
    if (ui_->completionCycleIndex >= 0)
    {
      auto& buf = ui_->inputPane.getBuffer();
      auto from = checkedConversion<std::size_t>(ui_->completionReplaceFrom);
      buf.replace(from, buf.size() - from, ui_->completionOriginal);
      ui_->cursorPos = ui_->completionReplaceFrom + checkedConversion<int>(ui_->completionOriginal.size());
    }
    ui_->clearCompletion();
    return true;
  }

  if (event == ftxui::Event::Tab && completer_ != nullptr)
  {
    auto& buf = ui_->inputPane.getBuffer();

    if (!ui_->completionCandidates.empty())
    {
      ui_->completionCycleIndex =
        (ui_->completionCycleIndex + 1) % checkedConversion<int>(ui_->completionCandidates.size());
      auto& c = ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)];
      auto from = checkedConversion<std::size_t>(ui_->completionReplaceFrom);
      buf.replace(from, buf.size() - from, c.text);
      ui_->cursorPos = ui_->completionReplaceFrom + checkedConversion<int>(c.text.size());
      return true;
    }

    auto result = completer_->complete(buf, ui_->cursorPos);
    while (result.candidates.size() == 1)
    {
      auto& c = result.candidates[0];
      bool needsContinuation = (c.kind == CompletionKind::path || c.kind == CompletionKind::object);

      if (needsContinuation)
      {
        buf.replace(checkedConversion<std::size_t>(result.replaceFrom),
                    checkedConversion<std::size_t>(result.replaceTo - result.replaceFrom),
                    c.text + ".");
        ui_->cursorPos = result.replaceFrom + checkedConversion<int>(c.text.size()) + 1;
        result = completer_->complete(buf, ui_->cursorPos);
      }
      else
      {
        buf.replace(checkedConversion<std::size_t>(result.replaceFrom),
                    checkedConversion<std::size_t>(result.replaceTo - result.replaceFrom),
                    c.text + " ");
        ui_->cursorPos = result.replaceFrom + checkedConversion<int>(c.text.size()) + 1;
        std::string description = !c.detail.empty() ? c.detail : c.display;
        std::string action;
        if (c.kind == CompletionKind::method)
        {
          action = (c.argCount == 0U) ? "run" : "args";
        }
        ui_->completionHint = std::move(description);
        ui_->completionHintAction = std::move(action);
        result.candidates.clear();
        break;
      }
    }

    if (result.candidates.size() > 1)
    {
      ui_->completionReplaceFrom = result.replaceFrom;
      ui_->completionOriginal = buf.substr(checkedConversion<std::size_t>(result.replaceFrom),
                                           checkedConversion<std::size_t>(result.replaceTo - result.replaceFrom));

      auto common = Completer::commonPrefix(result.candidates);
      if (common.size() > ui_->completionOriginal.size())
      {
        buf.replace(checkedConversion<std::size_t>(result.replaceFrom),
                    checkedConversion<std::size_t>(result.replaceTo - result.replaceFrom),
                    common);
        ui_->cursorPos = result.replaceFrom + checkedConversion<int>(common.size());
        ui_->completionOriginal = common;
      }

      ui_->completionCandidates = std::move(result.candidates);
      ui_->completionCycleIndex = -1;
    }
    return true;
  }

  if (event == ftxui::Event::TabReverse && !ui_->completionCandidates.empty())
  {
    auto& buf = ui_->inputPane.getBuffer();
    auto n = checkedConversion<int>(ui_->completionCandidates.size());
    ui_->completionCycleIndex = (ui_->completionCycleIndex - 1 + n) % n;
    auto& c = ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)];
    auto from = checkedConversion<std::size_t>(ui_->completionReplaceFrom);
    buf.replace(from, buf.size() - from, c.text);
    ui_->cursorPos = ui_->completionReplaceFrom + checkedConversion<int>(c.text.size());
    return true;
  }

  return false;
}

bool App::handleGlobalEvent(ftxui::Event event)
{
  // Ctrl+D is the direct, power-user exit path. Never gated by confirmation.
  if (event == ftxui::Event::Special({4}))
  {
    appendInfo("Shutting down...");
    exited_ = true;
    shutdownRequested_ = true;
    return true;
  }

  // k9s-style ESC handling: cancel the current context, then require a second press to
  // quit from the top level. Form-active and completion-active ESCs are already consumed
  // higher up in the dispatch chain, so here ESC always talks to the input pane.
  if (event == ftxui::Event::Escape)
  {
    auto& buf = ui_->inputPane.getBuffer();
    if (!buf.empty())
    {
      // Non-empty buffer: discard input. Also clear any transient UI state that the
      // buffer drove (error-reveal, completion cycle) so the user sees a clean prompt.
      buf.clear();
      ui_->cursorPos = 0;
      ui_->exitConfirmPending = false;
      ui_->clearCompletion();
      requestRedraw();
      return true;
    }
    if (ui_->exitConfirmPending)
    {
      appendInfo("Shutting down...");
      exited_ = true;
      shutdownRequested_ = true;
      return true;
    }
    ui_->exitConfirmPending = true;
    requestRedraw();
    return true;
  }

  // Any other user-input key dismisses an armed quit confirmation. Fall through so the
  // key performs its normal action. Internal events (the ~500 ms safety-net redraw posted
  // via Event::Custom, mouse events, cursor-position / shape responses from the terminal)
  // must not count as dismissal, or the confirmation row would flash away almost
  // immediately.
  if (ui_->exitConfirmPending && event != ftxui::Event::Custom && !event.is_mouse() && !event.is_cursor_position() &&
      !event.is_cursor_shape())
  {
    ui_->exitConfirmPending = false;
    requestRedraw();
  }

  if (event == ftxui::Event::F1)
  {
    if (onCommand_)
    {
      onCommand_("help");
    }
    return true;
  }

  if (event == ftxui::Event::CtrlY)
  {
    copySelectionToClipboard();
    return true;
  }

  // Pane toggles: TUI mode only.
  if (mode_ != Mode::repl)
  {
    if (event == ftxui::Event::F2)
    {
      if (ui_->bottomTab == UiState::BottomTab::logs)
      {
        ui_->logPane.clear();
      }
      else
      {
        ui_->eventPane.clear();
      }
      return true;
    }

    if (event == ftxui::Event::F3)
    {
      if (ui_->logPaneVisible && ui_->bottomTab == UiState::BottomTab::logs)
      {
        ui_->logPaneVisible = false;
        ui_->activePane = UiState::ActivePane::command;
      }
      else
      {
        ui_->logPaneVisible = true;
        ui_->bottomTab = UiState::BottomTab::logs;
        ui_->unreadLogCount = 0;
      }
      return true;
    }

    if (event == ftxui::Event::F4)
    {
      if (ui_->logPaneVisible && ui_->bottomTab == UiState::BottomTab::events)
      {
        ui_->logPaneVisible = false;
        ui_->activePane = UiState::ActivePane::command;
      }
      else
      {
        ui_->logPaneVisible = true;
        ui_->bottomTab = UiState::BottomTab::events;
        ui_->unreadEventCount = 0;
      }
      return true;
    }

    if (event == ftxui::Event::F5)
    {
      ui_->watchPaneVisible = !ui_->watchPaneVisible;
      return true;
    }
  }

  // Scroll: works in both modes. In TUI mode the active pane receives the scroll;
  // in REPL mode only the command pane exists.
  auto& activeOutput = (mode_ != Mode::repl && ui_->activePane == UiState::ActivePane::log && ui_->logPaneVisible)
                         ? ui_->logPane
                         : ui_->commandPane;

  if (event == ftxui::Event::PageUp)
  {
    activeOutput.scrollUp(pageScrollLines);
    return true;
  }
  if (event == ftxui::Event::PageDown)
  {
    activeOutput.scrollDown(pageScrollLines);
    return true;
  }

  return false;
}

bool App::handleMouseEvent(ftxui::Event event)
{
  if (!event.is_mouse())
  {
    return false;
  }

  auto& mouse = event.mouse();

  // Defer the clipboard copy until after RunOnce() so FTXUI has already folded
  // the Released event into its selection state. Skip releases that end a
  // separator drag, those aren't user selections.
  if (mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Released && mode_ != Mode::repl &&
      !ui_->logDragging && !ui_->watchDragging)
  {
    selectionCheckPending_ = true;
  }

  // Wheel handlers below return true on purpose: FTXUI selection is screen-
  // rectangle, not content-anchored, so if we preserved it across a scroll the
  // highlight would stay put while text slid beneath it. Returning true makes
  // ScreenInteractive::HandleSelection drop the selection, which is the less
  // confusing behavior and lets the user scroll without having to click first.

  // REPL mode: only handle scroll wheel on the command pane.
  if (mode_ == Mode::repl)
  {
    if (mouse.button == ftxui::Mouse::WheelUp)
    {
      ui_->commandPane.scrollUp(mouseWheelScrollLines);
      return true;
    }
    if (mouse.button == ftxui::Mouse::WheelDown)
    {
      ui_->commandPane.scrollDown(mouseWheelScrollLines);
      return true;
    }
    return false;
  }

  auto ts = ftxui::Terminal::Size();

  if (ui_->logDragging)
  {
    if (mouse.motion == ftxui::Mouse::Released)
    {
      ui_->logDragging = false;
      return true;
    }
    constexpr int windowBorderThickness = 2;
    ui_->logPaneHeight = std::max(3, mouse.y - windowBorderThickness + 1);
    return true;
  }
  if (ui_->watchDragging)
  {
    if (mouse.motion == ftxui::Mouse::Released)
    {
      ui_->watchDragging = false;
      return true;
    }
    ui_->watchPaneWidth = std::max(15, ts.dimx - mouse.x);
    return true;
  }

  if (ui_->logPaneVisible)
  {
    constexpr int windowBorderThickness = 2;
    int logBottomY = ui_->logPaneHeight + windowBorderThickness - 1;
    if (mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Pressed && mouse.y >= logBottomY - 1 &&
        mouse.y <= logBottomY + 1)
    {
      ui_->logDragging = true;
      return true;
    }
  }

  int watchWidth =
    ui_->watchPaneVisible ? ((ui_->watchPaneWidth > 0) ? ui_->watchPaneWidth : std::max(30, ts.dimx / 3)) : 0;
  watchWidth = std::max(15, std::min(watchWidth, ts.dimx - 30));
  int separatorX = ts.dimx - watchWidth;

  if (watchWidth > 0 && mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Pressed &&
      mouse.x >= separatorX - 1 && mouse.x <= separatorX + 1)
  {
    ui_->watchDragging = true;
    return true;
  }

  bool mouseOverWatch = watchWidth > 0 && mouse.x >= separatorX;

  if (mouseOverWatch)
  {
    if (mouse.button == ftxui::Mouse::WheelUp)
    {
      ui_->watchScrollRow = std::max(0, ui_->watchScrollRow - mouseWheelScrollLines);
      return true;
    }
    if (mouse.button == ftxui::Mouse::WheelDown)
    {
      ui_->watchScrollRow += mouseWheelScrollLines;
      return true;
    }
  }
  else
  {
    constexpr int windowBorderThickness = 2;
    int logTotalHeight = ui_->logPaneVisible ? (ui_->logPaneHeight + windowBorderThickness) : 0;
    bool mouseOverLog = ui_->logPaneVisible && mouse.y < logTotalHeight;
    auto& targetPane = mouseOverLog ? ui_->logPane : ui_->commandPane;

    if (mouse.button == ftxui::Mouse::WheelUp)
    {
      targetPane.scrollUp(mouseWheelScrollLines);
      return true;
    }
    if (mouse.button == ftxui::Mouse::WheelDown)
    {
      targetPane.scrollDown(mouseWheelScrollLines);
      return true;
    }
  }
  return false;
}

void App::tick()
{
  if (loop_ && !loop_->HasQuitted())
  {
    // Only redraw when something actually changed (mutation setters flip
    // needsRedraw_), while pending calls are animating their spinner, or
    // periodically as a safety net in case a mutation site is missed.
    // FTXUI will render on its own for user-driven events (keyboard, mouse,
    // terminal resize) without needing our Custom nudge.
    // The safety-net period also refreshes the status-bar clock and heartbeat.
    constexpr unsigned idleRedrawTicks = 15;  // 15 ticks @ 30 Hz = 500 ms
    const bool animating = activePendingCount_ > 0;
    const bool safetyNet = (idleTickCount_ >= idleRedrawTicks);
    if (needsRedraw_ || animating || safetyNet)
    {
      needsRedraw_ = false;
      idleTickCount_ = 0;
      screen_->PostEvent(ftxui::Event::Custom);
    }
    else
    {
      ++idleTickCount_;
    }
    loop_->RunOnce();

    if (selectionCheckPending_)
    {
      selectionCheckPending_ = false;
      auto sel = screen_->GetSelection();
      if (!sel.empty() && sel != lastCopiedSelection_)
      {
        clipboard::copy(sel);
        lastCopiedSelection_ = std::move(sel);
      }
    }
  }
  else
  {
    exited_ = true;
  }
}

void App::shutdown()
{
  loop_.reset();
  screen_.reset();
}

void App::copySelectionToClipboard()
{
  if (!screen_)
  {
    return;
  }
  auto text = screen_->GetSelection();
  if (text.empty())
  {
    appendInfo("Nothing selected. Drag the mouse over text, then press Ctrl+Y.");
    return;
  }
  clipboard::copy(text);
  lastCopiedSelection_ = text;
  appendInfo("Copied " + std::to_string(text.size()) + " chars to clipboard.");
}

}  // namespace sen::components::term
