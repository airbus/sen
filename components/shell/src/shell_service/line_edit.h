// === line_edit.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_LINE_EDIT_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_LINE_EDIT_H

#include "completer.h"
#include "history.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/kernel/component.h"
#include "terminal.h"

#include <functional>
#include <memory>

namespace sen::components::shell
{

class LineEditImpl;

using InputCallback = std::function<void(std::string, ::sen::kernel::RunApi&)>;

class LineEdit final
{
  SEN_MOVE_ONLY(LineEdit)

public:
  LineEdit(InputCallback&& inputCallback,
           std::function<void()>&& muteLogCallback,
           std::unique_ptr<Completer>&& completer,
           std::unique_ptr<History>&& history,
           Terminal* term,
           ::sen::kernel::RunApi& api,
           std::function<std::vector<std::string>()> availableSourcesFetcher);
  ~LineEdit();

public:
  void processEvents(::sen::kernel::RunApi& api);

public:
  void setPrompt(const std::string& prompt);
  void setWindowTitle(const std::string& windowTitle);
  void refresh() const;
  void clearScreen() const;

public:
  [[nodiscard]] History* getHistory() noexcept;
  [[nodiscard]] Terminal* getTerminal() noexcept;

private:
  std::unique_ptr<LineEditImpl> pimpl_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_LINE_EDIT_H
