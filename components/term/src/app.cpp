// === app.cpp =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "app.h"

// component
#include "app_renderers.h"
#include "arg_form.h"
#include "clipboard.h"
#include "completer.h"
#include "input_pane.h"
#include "output_pane.h"
#include "parse_utils.h"
#include "signal_stack.h"
#include "signature_renderer.h"
#include "styles.h"
#include "text_wrap.h"
#include "unicode.h"
#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"

// ftxui
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::term
{

namespace
{

/// Most visual lines of the input to draw at once. Drawing all of a long line leaves the output pane
/// with no rows and clips the line the cursor is on.
constexpr int maxInputRows = 6;

/// Bracketed paste. The terminal wraps pasted text in these two markers once the application asks
/// for them, which is the only way to tell a paste from someone typing the same bytes. FTXUI does not
/// ask and does not know them, so term enables the mode and reads the markers out of the unmatched
/// escape sequences FTXUI hands back.
constexpr auto* enableBracketedPaste = "\x1b[?2004h";
constexpr auto* disableBracketedPaste = "\x1b[?2004l";
constexpr auto* pasteStart = "\x1b[200~";
constexpr auto* pasteEnd = "\x1b[201~";

/// How long a paste may go without input before term assumes its end marker was lost. FTXUI flushes
/// an incomplete escape sequence after 50 ms, so a paste that stalls inside the six bytes of the end
/// marker -- over ssh, or through tmux -- delivers the marker in pieces and none of them matches. With
/// no bound the term stays in paste mode for ever, and in paste mode it swallows every key: there is
/// no gesture left that recovers it.
constexpr unsigned pasteIdleLimitTicks = 60;  // 60 ticks @ 30 Hz = 2 s

}  // namespace

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::size_t commandPaneMaxLines = 5000U;
constexpr int pageScrollLines = 10;
constexpr int mouseWheelScrollLines = 3;
constexpr int completionMinWidth = 40;
constexpr int completionMaxRows = 10;
constexpr int completionColPadding = 3;

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// UiState
//--------------------------------------------------------------------------------------------------------------

// App's own state, and App is the only thing that touches it: there is no invariant here for a
// private section to defend, and sixteen accessors would say less than this comment.
// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct App::UiState
{
  OutputPane commandPane;
  InputPane inputPane;

  // When set, the input area is replaced by the form. Consumed by the render pass and the event
  // handler; cleared on submit or cancel.
  std::optional<ArgForm> activeForm;

  // Completion state
  std::vector<Completion> completionCandidates;
  int completionCycleIndex = -1;   // -1 = not cycling, 0+ = current candidate
  int completionReplaceFrom = 0;   // start of the token being replaced
  int completionReplaceTo = 0;     // end of it, so cycling leaves the rest of the line alone
  std::string completionOriginal;  // the original token before cycling started
  // The buffer the offsets above were measured against, so an edit or a cursor move made by
  // anything other than completion can be noticed.
  std::size_t completionBufferSize = 0;
  int completionCursorPos = 0;
  // Completion accept hint (description + action). The action is always shown; description is
  // truncated on narrow terminals.
  std::string completionHint;
  std::string completionHintAction;

  // Armed by the first ESC on an empty input; a confirmation row shows while it is set. The policy
  // is at the ESC handler in handleGlobalEvent.
  bool exitConfirmPending = false;

  // Set once the line is full, so the notice is printed once per episode rather than once per
  // refused character -- a paste refused character by character would otherwise fill the output.
  bool inputFullReported = false;

  // Set when input was actually dropped, and cleared only when the user has edited the line since.
  // Submitting a line that was cut sends an argument the user never finished, and the failure is then
  // reported as a parse error naming text they did not type.
  bool inputTruncated = false;

  /// Record the span a completion candidate replaces, and the state of the buffer it was measured
  /// against.
  void recordCompletionSpan(const std::string& buffer, int from, int to)
  {
    completionReplaceFrom = from;
    completionReplaceTo = to;
    completionBufferSize = buffer.size();
    completionCursorPos = cursorPos;
  }

  /// Write `text` over the recorded span, leaving the rest of the line alone, and leave the span
  /// around what was written so the next candidate swaps with it. Both ends are clamped: a recorded
  /// offset can outlive the text it was measured against -- recalling a shorter history line does
  /// it -- and std::string::replace throws if pos > size(), which nothing between here and the
  /// kernel catches.
  void writeCompletion(std::string& buffer, const std::string& text)
  {
    const auto from = std::min(checkedConversion<std::size_t>(completionReplaceFrom), buffer.size());
    const auto to = std::clamp(checkedConversion<std::size_t>(completionReplaceTo), from, buffer.size());
    // Accepting a candidate on a full line would otherwise push past the bound, so `buffer.size() <=
    // maxLineBytes` would not be an invariant and the submit check could not rely on one. Cut on a
    // codepoint boundary, and measure the cursor from what was actually written: taking the candidate's
    // full length left the cursor past the end of the buffer, and the next key in the same batch of
    // events -- drained before any frame clamps it -- would index past the end and throw.
    const auto room = maxLineBytes - std::min(maxLineBytes, buffer.size() - (to - from));
    auto written = text.substr(0, std::min(text.size(), room));
    if (written.size() < text.size())
    {
      written = written.substr(0, prevCodepoint(text, written.size() + 1U));
      // Same hold as a cut keystroke: a candidate that did not fit is not what the user chose, and
      // submitting it sends a truncated object or method name.
      inputTruncated = true;
    }
    buffer.replace(from, to - from, written);
    cursorPos = checkedConversion<int>(from + written.size());
    recordCompletionSpan(buffer, checkedConversion<int>(from), cursorPos);
  }

  /// True while the recorded span still describes the buffer it was measured against. An edit or a
  /// cursor move from any other key invalidates it -- cycling on a stale offset appended the
  /// candidate to a truncated token instead of replacing it. Two changes that cancel out in both
  /// length and cursor cannot happen in one event.
  [[nodiscard]] bool completionSpanIsCurrent(const std::string& buffer) const
  {
    return buffer.size() == completionBufferSize && cursorPos == completionCursorPos;
  }

  void clearCompletion()
  {
    completionCandidates.clear();
    completionCycleIndex = -1;
    completionReplaceFrom = 0;
    completionReplaceTo = 0;
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
// NOLINTEND(misc-non-private-member-variables-in-classes)

//--------------------------------------------------------------------------------------------------------------
// App
//--------------------------------------------------------------------------------------------------------------

App::App(CommandHandler onCommand): onCommand_(std::move(onCommand)) {}

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

void App::appendLogOutput(std::string_view text)
{
  ui_->commandPane.appendText(text);
  requestRedraw();
}

void App::appendLogElement(ftxui::Element element)
{
  ui_->commandPane.appendElement(std::move(element));
  requestRedraw();
}

void App::appendEventElement(ftxui::Element element)
{
  ui_->commandPane.appendElement(std::move(element));
  requestRedraw();
}

void App::clearCommandPane()
{
  ui_->commandPane.clear();
  requestRedraw();
}

void App::setCompleter(Completer* completer) { completer_ = completer; }

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
  screen_ = std::make_unique<ftxui::App>(ftxui::App::Fullscreen());

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

  auto input = createInputComponent();
  auto renderer = createRenderer(input);
  auto eventHandler = createEventHandler(renderer);
  loop_ = std::make_unique<ftxui::Loop>(screen_.get(), eventHandler);

  // FTXUI installed the fatal signal handlers when the Loop went up, without SA_ONSTACK, which drops
  // the alternate signal stack a stack-overflow report depends on. Its handlers are kept; only the flag
  // goes back.
  std::ignore = restoreAltStackOnFatalHandlers();

  // Unverified on Windows, and worth knowing before trusting it there. term writes the DECSET and then
  // reads the markers out of the escape sequences FTXUI could not match. FTXUI 7.0.3 does set
  // ENABLE_VIRTUAL_TERMINAL_INPUT, so the markers *can* arrive, and if they do they reach the detector
  // -- but whether conhost or Windows Terminal emits them at all is unestablished, and the five session
  // tests that cover paste cannot run on Windows (no pty). If they never arrive, the behaviour is the
  // pre-bracketed-paste one: a multi-line paste runs a command per line. `pasteIdleLimitTicks` bounds
  // the other direction. Settling it takes one person pressing Ctrl+V in a Windows Terminal.
  //
  // After the Loop, which is where FTXUI installs the terminal: some terminals reset bracketed paste
  // when the alternate screen is entered, and asking first would leave the mechanism silently inert --
  // with the failure mode "pasted newlines submit again, and nothing says so". FTXUI writes through
  // std::cout as well, so sharing the stream is not a hazard.
  std::cout << enableBracketedPaste << std::flush;
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

  // Bounded by what the terminal can spare, not only by its own maximum: the input takes up to
  // maxInputRows, the rule one, and the output pane has to keep some rows or ftxui's hard shrink gives
  // it none and the user sees no output at all while a list is open.
  constexpr int minOutputRows = 3;
  const int spare = ftxui::Terminal::Size().dimy - maxInputRows - 1 - minOutputRows;
  // No floor: on a terminal too short to hold the input, the rule and three rows of output, the list
  // is not drawn at all rather than taking the row the promise reserved.
  const int rowBudget = std::min(completionMaxRows, spare);
  if (rowBudget < 1)
  {
    return ftxui::emptyElement();
  }

  int scrollOffset = 0;
  if (numRows > rowBudget && cycleIdx >= 0)
  {
    int selectedRow = cycleIdx % numRows;
    if (selectedRow >= rowBudget)
    {
      scrollOffset = selectedRow - rowBudget + 1;
    }
  }

  int visibleRows = std::min(numRows, rowBudget);

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

  return ftxui::hbox({ftxui::text(leftPad), ftxui::text(displayDescription) | styles::mutedText()});
}

//--------------------------------------------------------------------------------------------------------------
// init() helpers
//--------------------------------------------------------------------------------------------------------------

/// The keys the input line itself handles: motion, erasing, history and text. Split out of
/// createInputComponent, which was one function holding a renderer and sixteen key branches.
bool App::handleInputKey(const ftxui::Event& event)
{
  auto& buf = ui_->inputPane.getBuffer();

  if (event == ftxui::Event::Return)
  {
    if (ui_->inputTruncated)
    {
      // The line is not what the user meant to send: submitting it reports a parse failure that
      // names text they never typed.
      ui_->commandPane.scrollToBottom();
      appendInfo("This line was cut at " + std::to_string(maxLineBytes) +
                 " bytes, so it is not what you sent. Escape clears it; edit it to send it anyway.");
      return true;
    }
    ui_->cursorPos = 0;
    ui_->commandPane.scrollToBottom();
    ui_->inputPane.submit();
    return true;
  }
  if (event == ftxui::Event::ArrowUp)
  {
    ui_->inputPane.historyUp();
    ui_->cursorPos = checkedConversion<int>(buf.size());
    // The line is a recalled one now, not the truncated one the hold was about.
    ui_->inputTruncated = false;
    return true;
  }
  if (event == ftxui::Event::ArrowDown)
  {
    ui_->inputPane.historyDown();
    ui_->cursorPos = checkedConversion<int>(buf.size());
    return true;
  }
  // Every motion and erase below steps by codepoint, not by byte: a byte at a time splits a
  // multi-byte character and leaves the buffer invalid UTF-8, which then gets submitted.
  auto cursor = [this]() { return checkedConversion<std::size_t>(ui_->cursorPos); };
  auto setCursor = [this](std::size_t pos) { ui_->cursorPos = checkedConversion<int>(pos); };

  if (event == ftxui::Event::ArrowLeft)
  {
    setCursor(prevCodepoint(buf, cursor()));
    return true;
  }
  if (event == ftxui::Event::ArrowRight)
  {
    setCursor(nextCodepoint(buf, cursor()));
    return true;
  }
  if (event == ftxui::Event::ArrowLeftCtrl)
  {
    setCursor(prevWord(buf, cursor()));
    return true;
  }
  if (event == ftxui::Event::ArrowRightCtrl)
  {
    setCursor(nextWord(buf, cursor()));
    return true;
  }
  if (event == ftxui::Event::Home)
  {
    ui_->cursorPos = 0;
    return true;
  }
  if (event == ftxui::Event::End)
  {
    setCursor(buf.size());
    return true;
  }
  if (event == ftxui::Event::Backspace)
  {
    auto from = prevCodepoint(buf, cursor());
    buf.erase(from, cursor() - from);
    setCursor(from);
    ui_->inputTruncated = false;
    return true;
  }
  if (event == ftxui::Event::Delete)
  {
    auto to = nextCodepoint(buf, cursor());
    buf.erase(cursor(), to - cursor());
    // Erasing means the user has looked at the line, so stop holding the submit back.
    ui_->inputTruncated = false;
    return true;
  }
  // Ctrl+Backspace arrives as Ctrl+H in most terminals; Ctrl+W is the readline spelling.
  if (event == ftxui::Event::CtrlH || event == ftxui::Event::CtrlW)
  {
    auto from = prevWord(buf, cursor());
    buf.erase(from, cursor() - from);
    setCursor(from);
    ui_->inputTruncated = false;
    return true;
  }
  if (event.is_character())
  {
    // FTXUI has no bracketed paste, so pasted text arrives here one character at a time and this
    // is the only place it can be bounded. Without a bound, a line long enough to fill the screen
    // leaves the term alive but unable to act on any further key: it re-wraps the whole buffer
    // every frame and the layout gives the output pane no rows at all.
    if (buf.size() + event.character().size() > maxLineBytes)
    {
      if (!ui_->inputFullReported)
      {
        appendInfo("The command line is full at " + std::to_string(maxLineBytes) +
                   " bytes, and the rest was dropped. Escape clears the line.");
        ui_->inputFullReported = true;
      }
      ui_->inputTruncated = true;
      return true;
    }
    buf.insert(checkedConversion<std::size_t>(ui_->cursorPos), event.character());
    ui_->cursorPos += checkedConversion<int>(event.character().size());
    ui_->inputFullReported = false;
    return true;
  }
  return false;
}

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
        int promptWidth = ftxui::string_width(promptStr);
        int firstLineChars = std::max(1, termWidth - promptWidth);
        int contLineChars = std::max(1, termWidth);

        auto vlines = wrapToWidth(buf, firstLineChars, contLineChars);

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

        // Draw a bounded number of visual lines: the prompt's and the cursor's are always among
        // them. Drawing all of a long line collapses the output pane to nothing.
        const auto visible = visibleLines(checkedConversion<int>(vlines.size()), cursorLine, maxInputRows);
        // The two gaps are counted apart: one number for everything, printed at the first gap, told the
        // reader the rest of the command was above the cursor when most of it was below.
        const int hiddenAbove = (visible.indices.size() > 1U) ? visible.indices[1] - 1 : 0;
        const int hiddenBelow =
          visible.indices.empty() ? 0 : checkedConversion<int>(vlines.size()) - 1 - visible.indices.back();

        ftxui::Elements rows;
        for (std::size_t shown = 0; shown < visible.indices.size(); ++shown)
        {
          const int i = visible.indices[shown];
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
              // Length from the symbol, not a literal: a one-byte glyph would style two characters
              // after it, and a four-byte one would be cut in half and disappear.
              const auto symbolBytes = std::string_view(unicode::promptSymbol).size();
              parts.push_back(ftxui::text(std::string(promptStr.substr(symPos, symbolBytes))) | promptSymbolDeco);
              parts.push_back(ftxui::text(std::string(promptStr.substr(symPos + symbolBytes))));
            }
          }

          if (focused && i == cursorLine)
          {
            // The split is on a grapheme, not a codepoint. ftxui attaches a combining mark to the
            // character before it and silently drops one that begins an element, so a caret sitting on a
            // base character used to eat the accent after it, and a caret on the mark itself drew an
            // element of zero width -- an invisible cursor.
            const auto cursorByte = checkedConversion<std::size_t>(cursorCol);
            const auto afterCursor = nextGrapheme(segment, cursorByte);
            auto before = segment.substr(0, cursorByte);
            auto atCursor =
              (cursorByte < segment.size()) ? segment.substr(cursorByte, afterCursor - cursorByte) : std::string(" ");
            auto after = (cursorByte < segment.size()) ? segment.substr(afterCursor) : std::string();
            parts.push_back(ftxui::text(before));
            parts.push_back(ftxui::text(atCursor) | ftxui::inverted | ftxui::focus);
            parts.push_back(ftxui::text(after));
          }
          else
          {
            parts.push_back(ftxui::text(segment));
          }

          // The accept hint, after the cursor on the last visual line and only when it fits. Errors
          // deliberately do not appear here: they rarely fit after the query and get their own row
          // below the input.
          bool isLastLine = (i == checkedConversion<int>(vlines.size()) - 1);
          if (isLastLine && focused && !ui_->completionHintAction.empty())
          {
            int promptCells = vl.isFirst ? promptWidth : 0;
            int segCells = ftxui::string_width(segment);
            // The cursor sits past the last character only when its byte offset reaches the segment's
            // length in bytes -- comparing it against the segment's width in cells read true mid-line
            // on anything non-ASCII and added a cell the row does not use.
            int cursorExtraCell = (cursorLine == i && cursorCol >= checkedConversion<int>(segment.size())) ? 1 : 0;
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

          // Say what is not on screen, at each point where something was left out.
          const bool lastShown = (shown + 1U == visible.indices.size());
          const int hidden = (shown == 0U) ? hiddenAbove : (lastShown ? hiddenBelow : 0);
          if (hidden > 0)
          {
            const std::string where = (shown == 0U) ? " lines above" : " lines below";
            rows.push_back(
              ftxui::hbox({ftxui::text("  "),
                           ftxui::text(std::string(unicode::ellipsis) + " " + std::to_string(hidden) + where) |
                             styles::mutedText()}));
          }
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
    [this](const ftxui::Event& event) -> bool { return handleInputKey(event); });
}

ftxui::Component App::createRenderer(ftxui::Component wrappedInput)
{
  return ftxui::Renderer(
    wrappedInput,
    [this, wrappedInput]() -> ftxui::Element
    {
      auto commandArea = ui_->commandPane.render() | styles::inputBg() | styles::inputFg();
      if (toastVisible())
      {
        commandArea = ftxui::dbox(
          {std::move(commandArea), ftxui::vbox({ftxui::filler(), ftxui::hbox({ftxui::filler(), renderToast()})})});
      }
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

      // A rule between the scrolling output and the input, so the prompt does not read as just
      // another output line once the screen has filled. It needs the surrounding background as
      // well as a foreground: without it the line sits on the terminal's own background and
      // reads as a stripe across a light theme.
      auto rule = ftxui::separator() | styles::mutedText() | styles::inputBg();

      if (ui_->activeForm.has_value())
      {
        auto formArea = renderArgForm(*ui_->activeForm);
        return ftxui::vbox({commandArea | ftxui::flex, rule, std::move(formArea)}) | ftxui::flex;
      }

      ftxui::Elements rows;
      rows.push_back(commandArea | ftxui::flex);
      rows.push_back(rule);
      rows.push_back(completionArea | styles::completionBg() | styles::inputFg());
      rows.push_back(inputArea);
      return ftxui::vbox(std::move(rows)) | ftxui::flex;
    });
}

ftxui::Component App::createEventHandler(ftxui::Component renderer)
{
  return ftxui::CatchEvent(renderer,
                           [this](ftxui::Event event) -> bool
                           {
                             // Before everything else: inside a paste, keys are text.
                             if (handlePasteEvent(event))
                             {
                               return true;
                             }
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
                             return handleMouseEvent(event);
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
      auto inlineCommand =
        formatInlineInvocation(form.objectName(), form.methodName(), form.method().getArgs(), result.getValue());
      ui_->activeForm.reset();
      if (onCommand_)
      {
        onCommand_(inlineCommand);
      }
    }
    else
    {
      const auto& failure = result.getError();
      if (failure.fieldIndex < form.leafCount())
      {
        // Bounded: focusNext cycles, so an index that is not a leaf would spin here for ever, and
        // this runs on the component thread -- it would hang the kernel and every other component
        // loaded in it.
        for (std::size_t step = 0; step < form.leafCount() && form.focusedIndex() != failure.fieldIndex; ++step)
        {
          form.focusNext();
        }
      }
      else
      {
        // No field is at fault, so there is nothing to focus and the form would otherwise just
        // refuse in silence.
        appendElement(renderError("Cannot Submit", failure.message));
      }
    }
    return true;
  }
  if (event.is_character())
  {
    form.insertText(event.character());
    return true;
  }

  // Swallow the editing keys the form does not use. Falling through sent them to the input component,
  // which is not on screen while the form is: they edited a buffer nobody could see, and the cursor
  // clamp that would have caught an out-of-range position lives in the renderer that is skipped.
  // Harmless today only because the buffer is always empty when a form opens.
  // Anything else falls through, so the scroll handlers still see it.
  return event == ftxui::Event::Delete || event == ftxui::Event::Backspace || event == ftxui::Event::Home ||
         event == ftxui::Event::End || event == ftxui::Event::CtrlH || event == ftxui::Event::CtrlW ||
         event == ftxui::Event::ArrowLeftCtrl || event == ftxui::Event::ArrowRightCtrl;
}

void App::insertPastedText(std::string_view text)
{
  auto& buf = ui_->inputPane.getBuffer();
  if (buf.size() + text.size() > maxLineBytes)
  {
    if (!ui_->inputFullReported)
    {
      appendInfo("The command line is full at " + std::to_string(maxLineBytes) +
                 " bytes, and the rest of the paste was dropped. Escape clears the line.");
      ui_->inputFullReported = true;
    }
    ui_->inputTruncated = true;
    return;
  }

  buf.insert(checkedConversion<std::size_t>(ui_->cursorPos), text);
  ui_->cursorPos += checkedConversion<int>(text.size());
  ui_->inputFullReported = false;
  requestRedraw();
}

bool App::handlePasteEvent(ftxui::Event event)
{
  if (event == ftxui::Event::Special(pasteStart))
  {
    pasting_ = true;
    pastedNewlines_ = 0;
    pasteIdleTicks_ = 0;
    // The recorded completion span indexes the buffer the paste is about to change.
    ui_->clearCompletion();
    // Otherwise a paste that inserts nothing leaves the confirmation armed, and the next Escape -- which
    // the user means as "clear this" -- shuts the term down.
    ui_->exitConfirmPending = false;
    return true;
  }

  if (event == ftxui::Event::Special(pasteEnd))
  {
    pasting_ = false;
    if (pastedNewlines_ > 0)
    {
      // Say it rather than let the line look like something the user typed. Joining is the useful
      // behaviour for the common case, a single long command wrapped across lines.
      appendInfo("Pasted text held " + std::to_string(pastedNewlines_) +
                 (pastedNewlines_ == 1 ? " line break, joined with a space." : " line breaks, joined with spaces.") +
                 " Check the line before pressing Enter.");
    }
    pastedNewlines_ = 0;
    return true;
  }

  if (!pasting_)
  {
    return false;
  }

  // term posts Custom itself, to force a redraw. It is not pasted input, and treating it as such kept
  // the idle counter at zero thirty times a second, so the release below could never fire.
  if (event == ftxui::Event::Custom || event.is_mouse())
  {
    return false;
  }

  pasteIdleTicks_ = 0;

  // Escape ends a paste. Without it there is no key at all that leaves paste mode, because everything
  // below swallows what it does not recognise -- so a lost end marker meant killing the process.
  if (event == ftxui::Event::Escape)
  {
    pasting_ = false;
    pastedNewlines_ = 0;
    appendInfo("Paste cancelled. The text received so far is on the line.");
    return true;
  }

  // A form takes pasted text as text too, without its editing keys firing.
  const bool intoForm = ui_->activeForm.has_value();

  if (event == ftxui::Event::Return || event == ftxui::Event::Tab || event == ftxui::Event::TabReverse)
  {
    // A line break or a tab inside pasted text is a separator, not the key of the same name. Turning
    // it into a space keeps one wrapped command on one line; submitting on it ran every pasted line
    // as its own command.
    const auto sizeBefore = ui_->inputPane.getBuffer().size();
    if (intoForm)
    {
      static_cast<void>(ui_->activeForm->insertText(" "));
    }
    else
    {
      insertPastedText(" ");
    }
    // Only count a line break the line actually took: past the bound the space is dropped, and the
    // notice would otherwise claim joins that are not there.
    if (event == ftxui::Event::Return && (intoForm || ui_->inputPane.getBuffer().size() != sizeBefore))
    {
      ++pastedNewlines_;
    }
    return true;
  }

  if (event.is_character())
  {
    if (intoForm)
    {
      if (!ui_->activeForm->insertText(event.character()) && !ui_->inputFullReported)
      {
        appendInfo("This field is full at " + std::to_string(maxLineBytes) +
                   " bytes, and the rest of the paste was dropped.");
        ui_->inputFullReported = true;
      }
    }
    else
    {
      insertPastedText(event.character());
    }
    return true;
  }

  // Everything else in a paste -- an escape, a control byte, a cursor key the sender never meant to
  // send -- is dropped rather than acted on.
  return true;
}

bool App::handleCompletionEvent(ftxui::Event event)
{
  auto& buf = ui_->inputPane.getBuffer();

  // The recorded span indexes the buffer, so a change from any other key makes it meaningless.
  if (!ui_->completionCandidates.empty() && !ui_->completionSpanIsCurrent(buf))
  {
    ui_->clearCompletion();
  }

  const bool cycling = ui_->completionCycleIndex >= 0;
  const bool continuesAPath =
    cycling && event == ftxui::Event::Character(".") &&
    (ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)].kind ==
       CompletionKind::path ||
     ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)].kind ==
       CompletionKind::object);

  if (cycling && (event == ftxui::Event::Return || continuesAPath))
  {
    const auto& sel = ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)];

    if (continuesAPath)
    {
      ui_->writeCompletion(buf, sel.text + ".");
      ui_->clearCompletion();

      auto nextResult = completer_->complete(buf, ui_->cursorPos);
      if (!nextResult.candidates.empty())
      {
        ui_->recordCompletionSpan(buf, nextResult.replaceFrom, nextResult.replaceTo);
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
      ui_->writeCompletion(buf, sel.text + " ");
      ui_->clearCompletion();
      ui_->completionHint = std::move(description);
      ui_->completionHintAction = std::move(action);
    }
    return true;
  }

  // Every key that edits the line or moves within it ends the completion, including the accept hint
  // left over from the last one. Cursor motion and word erase were missing, so the hint stayed on
  // screen describing a token the cursor had left.
  const bool editsOrMoves = event.is_character() || event == ftxui::Event::Return || event == ftxui::Event::Backspace ||
                            event == ftxui::Event::Delete || event == ftxui::Event::ArrowUp ||
                            event == ftxui::Event::ArrowDown || event == ftxui::Event::ArrowLeft ||
                            event == ftxui::Event::ArrowRight || event == ftxui::Event::ArrowLeftCtrl ||
                            event == ftxui::Event::ArrowRightCtrl || event == ftxui::Event::Home ||
                            event == ftxui::Event::End || event == ftxui::Event::CtrlH || event == ftxui::Event::CtrlW;
  if (editsOrMoves &&
      (!ui_->completionCandidates.empty() || !ui_->completionHint.empty() || !ui_->completionHintAction.empty()))
  {
    ui_->clearCompletion();
  }

  if (event == ftxui::Event::Escape && !ui_->completionCandidates.empty())
  {
    if (ui_->completionCycleIndex >= 0)
    {
      ui_->writeCompletion(buf, ui_->completionOriginal);
    }
    ui_->clearCompletion();
    return true;
  }

  if (event == ftxui::Event::Tab && completer_ != nullptr)
  {
    if (!ui_->completionCandidates.empty())
    {
      ui_->completionCycleIndex =
        (ui_->completionCycleIndex + 1) % checkedConversion<int>(ui_->completionCandidates.size());
      ui_->writeCompletion(buf,
                           ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)].text);
      return true;
    }

    auto result = completer_->complete(buf, ui_->cursorPos);
    while (result.candidates.size() == 1)
    {
      auto& c = result.candidates[0];
      bool needsContinuation = (c.kind == CompletionKind::path || c.kind == CompletionKind::object);

      // Through writeCompletion, not a raw replace: it is the only writer that clamps the recorded span
      // against the buffer and holds the line to maxLineBytes. Writing here directly meant one Tab on a
      // nearly full line pushed past the bound, and the over-long line then submitted and was dropped
      // from history without a word.
      if (needsContinuation)
      {
        ui_->recordCompletionSpan(buf, result.replaceFrom, result.replaceTo);
        ui_->writeCompletion(buf, c.text + ".");
        result = completer_->complete(buf, ui_->cursorPos);
      }
      else
      {
        ui_->recordCompletionSpan(buf, result.replaceFrom, result.replaceTo);
        ui_->writeCompletion(buf, c.text + " ");
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
      ui_->recordCompletionSpan(buf, result.replaceFrom, result.replaceTo);
      ui_->completionOriginal = buf.substr(checkedConversion<std::size_t>(result.replaceFrom),
                                           checkedConversion<std::size_t>(result.replaceTo - result.replaceFrom));

      auto common = Completer::commonPrefix(result.candidates);
      if (common.size() > ui_->completionOriginal.size())
      {
        ui_->writeCompletion(buf, common);
        ui_->completionOriginal = common;
      }

      ui_->completionCandidates = std::move(result.candidates);
      ui_->completionCycleIndex = -1;
    }
    return true;
  }

  if (event == ftxui::Event::TabReverse && !ui_->completionCandidates.empty())
  {
    auto n = checkedConversion<int>(ui_->completionCandidates.size());
    ui_->completionCycleIndex = (ui_->completionCycleIndex - 1 + n) % n;
    ui_->writeCompletion(buf,
                         ui_->completionCandidates[checkedConversion<std::size_t>(ui_->completionCycleIndex)].text);
    return true;
  }

  return false;
}

bool App::handleGlobalEvent(ftxui::Event event)
{
  // Ctrl+D exits, but only from an empty line, as every shell does. FTXUI has no bracketed paste,
  // so a 0x04 anywhere in pasted text arrives as this event: unconditional, it quit the term in the
  // middle of a paste with a half-typed line on screen.
  if (event == ftxui::Event::Special({4}))
  {
    if (!ui_->inputPane.getBuffer().empty())
    {
      appendInfo("Ctrl+D exits from an empty line. Clear it first, or use 'exit'.");
      return true;
    }
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
      // The refusal message tells the user to press Escape, so Escape has to release the hold.
      ui_->inputTruncated = false;
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

  // The scroll position is a fraction of the content, so the first press can land inside the rows
  // already on screen and move nothing.
  auto& activeOutput = ui_->commandPane;

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
  if (mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Released)
  {
    selectionCheckPending_ = true;
  }

  // Wheel handlers below return true on purpose: FTXUI selection is screen-
  // rectangle, not content-anchored, so if we preserved it across a scroll the
  // highlight would stay put while text slid beneath it. Returning true makes
  // App::HandleSelection drop the selection, which is the less
  // confusing behavior and lets the user scroll without having to click first.

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

/// How long a toast stays up. Long enough to read a short line without the eye having to hurry, short
/// enough that it is gone before it becomes part of the furniture.
constexpr auto toastDuration = std::chrono::milliseconds(1500);

void App::showToast(std::string text)
{
  toastText_ = std::move(text);
  toastUntil_ = std::chrono::steady_clock::now() + toastDuration;
  requestRedraw();
}

bool App::toastVisible() const { return !toastText_.empty() && std::chrono::steady_clock::now() < toastUntil_; }

ftxui::Element App::renderToast() const
{
  return ftxui::borderRounded(ftxui::text(" " + toastText_ + " ") | styles::inputFg()) | styles::completionBg();
}

void App::tick()
{
  if (loop_ && !loop_->HasQuitted())
  {
    // Only redraw when something changed (every mutation setter flips needsRedraw_), while a
    // pending call animates its spinner, or periodically as a safety net in case a mutation site
    // was missed. FTXUI renders on its own for keyboard, mouse and resize events, so those need no
    // nudge from us.
    // End a paste whose end marker never arrived, so the term cannot be left swallowing every key.
    if (pasting_ && ++pasteIdleTicks_ >= pasteIdleLimitTicks)
    {
      pasting_ = false;
      pasteIdleTicks_ = 0;
      pastedNewlines_ = 0;
      appendInfo("The paste did not finish, so it was ended here. Check the line before pressing Enter.");
      requestRedraw();
    }

    // Re-asserted rather than set once. FTXUI's SIGTSTP handling uninstalls and reinstalls the terminal
    // around the stop, and its reinstall knows nothing about bracketed paste -- so after Ctrl+Z and `fg`
    // the mode was off, `pasting_` was never set again, and a multi-line paste went back to running one
    // command per line. Eight bytes a second is cheaper than any way of noticing the resume.
    constexpr unsigned pasteModeReassertTicks = 30;  // once a second at 30 Hz
    if (++pasteModeTicks_ >= pasteModeReassertTicks)
    {
      pasteModeTicks_ = 0;
      std::cout << enableBracketedPaste << std::flush;
    }

    // The clipboard helper runs on its own thread, so its failure arrives after the copy did. Showing
    // it here is the only honest report: the copy was announced before anything had been written.
    if (auto failure = clipboard::takeFailure(); failure.has_value())
    {
      appendInfo(*failure);
    }

    // Nothing drives a toast but the clock, so the frames it is up have to be requested, and one more
    // after it goes to rub it out.
    const bool toastUp = toastVisible();
    if (toastUp || toastWasVisible_)
    {
      requestRedraw();
    }
    toastWasVisible_ = toastUp;

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
        copyToClipboardAndReport(std::move(sel));
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
  // Nothing to undo if the terminal was never installed. Without this, destroying an App that never
  // ran -- which a unit test does -- wrote a bracketed-paste escape onto the test runner's stdout.
  if (!screen_)
  {
    return;
  }

  std::cout << disableBracketedPaste << std::flush;
  loop_.reset();
  screen_.reset();
}

void App::copyToClipboardAndReport(std::string text)
{
  // Codepoints, not bytes: text.size() over-reports anything non-ASCII.
  const auto characters = codepointCount(text);
  clipboard::copy(text);
  lastCopiedSelection_ = std::move(text);
  showToast("Copied " + std::to_string(characters) + (characters == 1 ? " character" : " characters"));
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
  copyToClipboardAndReport(std::move(text));
}

}  // namespace sen::components::term
