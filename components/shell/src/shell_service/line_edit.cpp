// === line_edit.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "line_edit.h"

/// component
#include "completer.h"
#include "history.h"
#include "styles.h"
#include "terminal.h"
#include "util.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/kernel/component_api.h"

// std
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::shell
{

class Line final
{
public:
  Line() = default;
  ~Line() = default;

public:
  SEN_COPY_CONSTRUCT(Line) = default;
  SEN_MOVE_CONSTRUCT(Line) = default;
  SEN_COPY_ASSIGN(Line) = default;
  SEN_MOVE_ASSIGN(Line) = default;

public:
  [[nodiscard]] size_t getPos() const noexcept { return pos_; }

  [[nodiscard]] size_t getPreviousPos() const noexcept { return prevPos_; }

  void increasePos() noexcept
  {
    prevPos_ = pos_;
    ++pos_;
  }

  void decreasePos() noexcept
  {
    prevPos_ = pos_;
    --pos_;
  }

  void setPos(size_t p) noexcept
  {
    prevPos_ = pos_;
    pos_ = p;
  }

  void reset()
  {
    buffer_.clear();
    pos_ = 0U;
    prevPos_ = 0U;
  }

  [[nodiscard]] const std::string& getBuffer() const noexcept { return buffer_; }

  [[nodiscard]] std::string& getBuffer() noexcept { return buffer_; }

private:
  std::string buffer_;   // Edited line buffer.
  size_t pos_ = 0U;      // Current cursor position.
  size_t prevPos_ = 0U;  // Previous cursor position.
};

struct Cell
{
  std::string value;
  Style style = cellStyle;
};

using Row = std::vector<Cell>;
using Table = std::vector<Row>;

//--------------------------------------------------------------------------------------------------------------
// LineEditImpl
//--------------------------------------------------------------------------------------------------------------

class LineEditImpl final
{
  SEN_NOCOPY_NOMOVE(LineEditImpl)

public:
  LineEditImpl(InputCallback&& inputCallback,
               std::function<void()>&& muteLogCallback,
               std::unique_ptr<Completer>&& completer,
               std::unique_ptr<History>&& history,
               Terminal* term,
               kernel::RunApi& api,
               std::function<std::vector<std::string>()> availableSourcesFetcher)
    : inputCallback_(std::move(inputCallback))
    , muteLogCallback_(std::move(muteLogCallback))
    , completer_(std::move(completer))
    , history_(std::move(history))
    , term_(term)
    , hostName_(getHostName())
    , api_(api)
    , availableSourcesFetcher_(std::move(availableSourcesFetcher))
  {
  }

  ~LineEditImpl() = default;

public:
  void setWindowTitle(const std::string& windowTitle)
  {
    if (!windowTitle.empty())
    {
      term_->setWindowTitle(windowTitle);
    }
  }

  void setPrompt(std::string_view prompt)
  {
    prompt_ = prompt;
    promptLength_ = std::string_view("sen:").length() + hostName_.length() + 2U;

    if (!prompt_.empty())
    {
      promptLength_ += prompt_.length() + 1U;
    }
  }

  [[nodiscard]] Terminal* getTerminal() { return term_; }

  void clearScreen() { term_->clearScreen(); }

  void processEvents(kernel::RunApi& api)
  {
    constexpr auto duration = Duration::fromHertz(30.0);

    auto key = Key::keyNone;
    char character = 0U;

    for (bool done = false; !done;)
    {
      if (!term_->getchar(key, character, duration))
      {
        break;
      }

      processKey(key, character, done, std::move(api));
    }
  }

  [[nodiscard]] History* getHistory() noexcept { return history_.get(); }

  void refresh() { refreshLine(curLine_); }

private:
  void processKey(Key key, char character, bool& done, kernel::RunApi&& api)
  {
    std::ignore = done;

    switch (key)
    {
      case Key::keyCharacter:
        editInsert(curLine_, character);
        break;
      case Key::keyCtrlT:
        editSwapCharacters(curLine_);
        break;
      case Key::keyCtrlB:
        editMoveLeft(curLine_);
        break;
      case Key::keyCtrlF:
        editMoveRight(curLine_);
        break;
      case Key::keyCtrlP:
        editHistoryPrevious(curLine_);
        break;
      case Key::keyCtrlN:
        editHistoryNext(curLine_);
        break;
      case Key::keyCtrlU:
        editClearLine(curLine_);
        break;
      case Key::keyCtrlK:
        editClearRemainingLine(curLine_);
        break;
      case Key::keyCtrlA:
        editHome(curLine_);
        break;
      case Key::keyCtrlE:
        editEnd(curLine_);
        break;
      case Key::keyCtrlL:
        editClearScreen(curLine_);
        break;
      case Key::keyCtrlD:
      {
        if (curLine_.getBuffer().empty())
        {
          // simulate "exit"
          curLine_.getBuffer() = "shutdown";
          refreshLine(curLine_);
          auto buffer = editEnter(curLine_);
          inputCallback_(buffer, api);
        }
        else
        {
          editDelete(curLine_);
        }

        break;
      }
      case Key::keyHome:
        editHome(curLine_);
        break;
      case Key::keyEnd:
        editEnd(curLine_);
        break;
      case Key::keyArrowLeft:
        editMoveLeft(curLine_);
        break;
      case Key::keyArrowRight:
        editMoveRight(curLine_);
        break;
      case Key::keyArrowUp:
        editHistoryPrevious(curLine_);
        break;
      case Key::keyArrowDown:
        editHistoryNext(curLine_);
        break;
      case Key::keyDelete:
        editDelete(curLine_);
        break;
      case Key::keyTab:
        editComplete(curLine_);
        break;
      case Key::keyBackspace:
        editBackspace(curLine_);
        break;
      case Key::keyEnter:
      {
        auto buffer = editEnter(curLine_);
        inputCallback_(buffer, api);
      }
      break;

      case Key::keyF6:
      {
        muteLogCallback_();
      }
      break;

      default:
        break;
    }
  }

  void editInsert(Line& l, char c)
  {
    clearTable();

    if (l.getPos() == l.getBuffer().length())
    {
      l.getBuffer() += c;
      l.increasePos();
      refreshLine(l);
    }
    else
    {
      l.getBuffer().insert(l.getPos(), 1, c);
      l.increasePos();
      refreshLine(l);
    }
  }

  void editClearScreen(Line& l)
  {
    term_->clearScreen();
    writePrompt();
    refreshLine(l);
  }

  void editSwapCharacters(Line& l)
  {
    const size_t pos = l.getPos();

    if (pos > 0U && !l.getBuffer().empty())
    {
      char tmp = l.getBuffer()[pos];
      l.getBuffer()[pos] = l.getBuffer()[pos - 1];
      l.getBuffer()[pos - 1] = tmp;
      refreshLine(l);
    }
  }

  void editBackspace(Line& l)
  {
    if (l.getPos() > 0U && !l.getBuffer().empty())
    {
      clearTable();
      l.decreasePos();
      l.getBuffer().erase(l.getPos(), 1);
      refreshLine(l);
    }
  }

  void editDelete(Line& l)
  {
    if (!l.getBuffer().empty() && l.getPos() < l.getBuffer().length())
    {
      clearTable();
      l.getBuffer().erase(l.getPos(), 1);
      refreshLine(l);
    }
  }

  void editClearLine(Line& l)
  {
    clearTable();
    l.getBuffer().clear();
    l.setPos(0U);
    refreshLine(l);
  }

  void editClearRemainingLine(Line& l)
  {
    if (!l.getBuffer().empty() && l.getPos() < l.getBuffer().length())
    {
      clearTable();
      l.getBuffer().erase(l.getPos(), std::string::npos);
      refreshLine(l);
    }
  }

  void editMoveLeft(Line& l)
  {
    if (l.getPos() > 0U)
    {
      l.decreasePos();
      refreshLine(l);
    }
  }

  void editMoveRight(Line& l)
  {
    if (l.getPos() != l.getBuffer().length())
    {
      l.increasePos();
      refreshLine(l);
    }
  }

  void editHistoryNext(Line& l)
  {
    clearTable();

    if (!history_)
    {
      return;
    }

    // move the index
    std::string line = history_->nextLine();

    // set the current buffer with the history_ element
    l.getBuffer() = line;
    l.setPos(l.getBuffer().length());

    // redraw
    refreshLine(l);
  }

  void editHistoryPrevious(Line& l)
  {
    clearTable();

    if (!history_)
    {
      return;
    }

    // move the index
    std::string line = history_->prevLine();

    // set the current buffer with the history element
    l.getBuffer() = line;
    l.setPos(l.getBuffer().length());

    // redraw
    refreshLine(l);
  }

  void editHome(Line& l)
  {
    // update the position
    l.setPos(0U);

    // redraw
    refreshLine(l);
  }

  void editEnd(Line& l)
  {
    // update the position
    l.setPos(l.getBuffer().length());

    // redraw
    refreshLine(l);
  }

  [[nodiscard]] std::string editEnter(Line& l)
  {
    std::string result = l.getBuffer();

    clearTable();
    refreshLine(l, false);
    term_->newLine();
    l.reset();

    if (result.empty())
    {
      refreshLine(curLine_);
    }

    return result;
  }

  void editComplete(Line& l)
  {
    std::string commonPrefix;
    CompletionList completions;
    completer_->getCompletions(l.getBuffer(), api_, availableSourcesFetcher_, completions, commonPrefix);

    if (completions.empty() && commonPrefix.empty())
    {
      return;
    }

    if (completions.size() == 1)
    {
      l.getBuffer().append(completions.begin()->get());
      l.setPos(l.getBuffer().length());

      // redraw
      refreshLine(l, true);
    }
    else
    {
      // create the table
      const size_t cols = 6;
      Table table;

      // fill the table
      Row row;
      for (const auto& completion: completions)
      {
        std::string value = l.getBuffer() + completion.get();

        if (size_t lastPoint = value.find_last_of('.'); lastPoint != std::string::npos)
        {
          value = value.substr(lastPoint + 1);
        }

        if (size_t lastSpace = value.find_last_of(' '); lastSpace != std::string::npos)
        {
          value = value.substr(lastSpace + 1);
        }

        row.push_back({value, completion.getStyle()});

        if (row.size() == cols)
        {
          table.push_back(row);
          row.clear();
        }
      }

      // add the last row
      if (!row.empty())
      {
        table.push_back(row);
      }

      // partial complete
      if (l.getBuffer() != commonPrefix)
      {
        l.getBuffer() = commonPrefix;
        l.setPos(l.getBuffer().length());
      }

      // draw table
      writeTable(table, (promptLength_ > 5 ? 2 : promptLength_));  // NOLINT(readability-magic-numbers)

      term_->moveCursorAllLeft();
      for (size_t i = 0U; i < tableRows_; ++i)
      {
        term_->moveCursorUp();
      }

      // redraw
      refreshLine(l);
    }
  }

  void refreshLine(const Line& l, bool highlight = false)
  {
    // make the cursor invisible
    term_->hideCursor();

    // get the terminal size
    uint32_t rows = 80;
    uint32_t cols = 80;
    term_->getSize(rows, cols);

    auto absLength = static_cast<uint32_t>(promptLength_ + l.getBuffer().length());
    auto absCurrPos = static_cast<uint32_t>(promptLength_ + l.getPos());
    auto absPrevPos = static_cast<uint32_t>(promptLength_ + l.getPreviousPos());

    // 0 based index
    auto currLine = static_cast<uint32_t>(floorf(static_cast<float>(absCurrPos) / static_cast<float>(cols)));
    auto prevLine = static_cast<uint32_t>(floorf(static_cast<float>(absPrevPos) / static_cast<float>(cols)));

    // count of used lines
    auto usedLines = static_cast<uint32_t>(ceilf(static_cast<float>(absLength) / static_cast<float>(cols)));

    if (currLine != prevLine)
    {
      if (currLine < prevLine)
      {
        if ((absCurrPos + 1) % cols == 0U)
        {
          term_->moveCursorUp();
          term_->moveCursorRight(cols);
        }
      }
      else
      {
        if (absCurrPos % cols == 0U)
        {
          term_->newLine();
          term_->moveCursorDown();
          term_->moveCursorAllLeft();
        }
      }
    }

    // go to the initial line
    if (currLine != 0U)
    {
      term_->moveCursorUp(currLine);
    }

    term_->moveCursorAllLeft();

    // write prompt
    printPromptText();

    // write the current buffer content
    {
      if (highlight)
      {
        term_->cprint(highlightBufferStyle, l.getBuffer());
      }
      else
      {
        term_->cprint(defaultBufferStyle, l.getBuffer());
      }
    }

    term_->clearRemainingCurrentLine();

    // position the cursor
    {
      if (currLine != usedLines)
      {
        term_->moveCursorUp(usedLines - currLine - 1);
      }

      term_->moveCursorAllLeft();
      term_->moveCursorRight(absCurrPos % cols);
    }

    // make the cursor visible again
    term_->showCursor();
  }

  void writePrompt()
  {
    // make the cursor invisible
    term_->hideCursor();

    // cursor to line start
    term_->moveCursorAllLeft();

    // write prompt
    printPromptText();

    // make the cursor visible again
    term_->showCursor();
  }

  void printPromptText()
  {
    term_->cprint(promptHeaderStyle, "sen");
    term_->cprint(promptSeparatorStyle, ":");
    term_->cprint(promptHostStyle, getHostName());

    if (!prompt_.empty())
    {
      term_->cprint(promptSeparatorStyle, "/");
      term_->cprint(promptAppNameStyle, prompt_);
    }

    term_->cprint(promptSeparatorStyle, "> ");
  }

  void removeColumn(const Table& table, Table& newTable) const
  {
    if (table.empty())
    {
      return;
    }

    newTable.clear();

    Row elements;

    for (const auto& elem: table)
    {
      elements.insert(elements.end(), elem.begin(), elem.end());
    }

    size_t oldColumnCount = table[0U].size();
    size_t newColumnCount = oldColumnCount - 1;

    Row currentRow;
    for (const auto& elem: elements)
    {
      currentRow.push_back(elem);

      if (currentRow.size() == newColumnCount)
      {
        newTable.push_back(currentRow);
        currentRow.clear();
      }
    }

    if (!currentRow.empty())
    {
      newTable.push_back(currentRow);
    }
  }

  [[nodiscard]] std::vector<size_t> getMaxWidths(const Table& table) const
  {
    size_t cols = table[0U].size();

    // calc the max widths
    std::vector<size_t> maxWidths(cols, 0U);

    for (const auto& row: table)
    {
      for (size_t j = 0U; j < row.size(); ++j)
      {
        maxWidths[j] = (maxWidths[j] < row[j].value.length() ? row[j].value.length() : maxWidths[j]);
      }
    }

    return maxWidths;
  }

  void writeTable(const Table& srcTable, size_t indentation)
  {
    if (srcTable.empty())
    {
      return;
    }

    size_t newTableRows = 0U;

    // make the cursor invisible
    term_->hideCursor();

    uint32_t screenRows = 0;
    uint32_t screenCols = 0;
    term_->getSize(screenRows, screenCols);

    Table table = srcTable;
    for (;;)
    {
      size_t cols = table[0U].size();

      // calc the max widths
      std::vector<size_t> maxWidths = getMaxWidths(table);

      size_t totalWidth = indentation + cols * 3 + 2;
      for (const auto& maxWidth: maxWidths)
      {
        totalWidth += maxWidth;
      }

      if (totalWidth > screenCols && cols > 1U)
      {
        Table temp;
        removeColumn(table, temp);
        table = temp;
      }
      else
      {
        break;
      }
    }

    std::vector<size_t> maxWidths = getMaxWidths(table);
    std::string indentStr(indentation, ' ');

    // go to the next line
    newTableRows++;
    if (newTableRows > tableRows_)
    {
      term_->newLine();
    }
    else
    {
      term_->moveCursorDown();
      term_->moveCursorAllLeft();
      term_->clearRemainingCurrentLine();
    }

    // write rows
    for (size_t i = 0U; i < table.size(); ++i)
    {
      // write indentation
      term_->print(indentStr);

      const Row& row = table[i];
      if (!row.empty())
      {
        for (size_t j = 0U; j < row.size(); ++j)
        {
          const std::string& cellValue = row[j].value;

          // write cell contents
          term_->cprint(row[j].style, cellValue);

          // write padding
          size_t width = maxWidths[j] - cellValue.length() + 3;
          term_->print(std::string(width, ' '));
        }

        if (i != table.size() - 1)
        {
          // go to the next line
          newTableRows++;
          if (newTableRows > tableRows_)
          {
            term_->newLine();
          }
          else
          {
            term_->moveCursorDown();
            term_->moveCursorAllLeft();
            term_->clearRemainingCurrentLine();
          }
        }
      }
    }

    tableRows_ = newTableRows;

    // make the cursor visible again
    term_->showCursor();
  }

  void clearTable()
  {
    if (tableRows_ == 0U)
    {
      return;
    }

    term_->saveCursorPosition();

    // make the cursor invisible
    term_->hideCursor();
    term_->moveCursorAllLeft();

    for (size_t i = 0U; i < tableRows_; ++i)
    {
      term_->moveCursorDown();
      term_->clearRemainingCurrentLine();
    }

    // make the cursor visible again
    term_->showCursor();

    term_->restoreCursorPosition();

    tableRows_ = 0U;
  }

private:
  InputCallback inputCallback_;
  std::function<void()> muteLogCallback_;
  std::unique_ptr<Completer> completer_;
  std::unique_ptr<History> history_;
  Terminal* term_;
  std::string hostName_;
  std::string prompt_;
  std::size_t promptLength_ = 0U;
  size_t tableRows_ = 0U;
  Line curLine_;
  kernel::RunApi& api_;
  SourcesFetcher availableSourcesFetcher_;
};

//--------------------------------------------------------------------------------------------------------------
// LineEdit
//--------------------------------------------------------------------------------------------------------------

LineEdit::LineEdit(InputCallback&& inputCallback,
                   std::function<void()>&& muteLogCallback,
                   std::unique_ptr<Completer>&& completer,
                   std::unique_ptr<History>&& history,
                   Terminal* term,
                   kernel::RunApi& api,
                   SourcesFetcher availableSourcesFetcher)
  : pimpl_(std::make_unique<LineEditImpl>(std::move(inputCallback),
                                          std::move(muteLogCallback),
                                          std::move(completer),
                                          std::move(history),
                                          term,
                                          api,
                                          std::move(availableSourcesFetcher)))
{
}

LineEdit::~LineEdit() = default;

History* LineEdit::getHistory() noexcept { return pimpl_->getHistory(); }

void LineEdit::setPrompt(const std::string& prompt) { pimpl_->setPrompt(prompt); }

Terminal* LineEdit::getTerminal() noexcept { return pimpl_->getTerminal(); }

void LineEdit::setWindowTitle(const std::string& windowTitle) { pimpl_->setWindowTitle(windowTitle); }

void LineEdit::processEvents(kernel::RunApi& api) { pimpl_->processEvents(api); }

void LineEdit::clearScreen() const { pimpl_->clearScreen(); }

void LineEdit::refresh() const { pimpl_->refresh(); }

}  // namespace sen::components::shell
