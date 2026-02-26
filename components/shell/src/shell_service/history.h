// === history.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_HISTORY_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_HISTORY_H

#include "sen/core/base/class_helpers.h"

#include <filesystem>
#include <list>
#include <string>

namespace sen::components::shell
{

class History final
{
public:
  SEN_MOVE_ONLY(History)

public:
  static constexpr auto maxHistorySize = 40U;

public:
  History();
  ~History() = default;

public:
  void load();
  void addLine(std::string_view line);
  [[nodiscard]] std::string nextLine();
  [[nodiscard]] std::string prevLine();
  [[nodiscard]] const std::list<std::string>& getLines() const;

private:
  std::list<std::string> lines_;
  std::list<std::string>::iterator currentLine_;
  std::filesystem::path currentFileName_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_HISTORY_H
