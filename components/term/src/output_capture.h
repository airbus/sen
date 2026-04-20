// === output_capture.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_OUTPUT_CAPTURE_H
#define SEN_COMPONENTS_TERM_SRC_OUTPUT_CAPTURE_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"

// std
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace sen::components::term
{

/// Captures stderr by redirecting the file descriptor through a pipe.
/// Captured output is forwarded to a callback (typically the log pane).
/// stdout cannot be captured because FTXUI uses it for rendering (escape sequences,
/// cursor positioning, terminal size queries). Components should use spdlog instead.
class OutputCapture final
{
  SEN_NOCOPY_NOMOVE(OutputCapture)

public:
  using OutputCallback = sen::std_util::move_only_function<void(const std::string& line)>;

  explicit OutputCapture(OutputCallback callback);
  ~OutputCapture();

  /// Drain any pending captured output. Call periodically from the main loop.
  void drain();

private:
  void readerThread(int pipeFd);

private:
  OutputCallback callback_;
  std::atomic_bool running_ {true};

  int savedStderr_ = -1;
  int stderrPipeRead_ = -1;
  std::thread stderrReader_;

  std::mutex mutex_;
  std::vector<std::string> pendingLines_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_OUTPUT_CAPTURE_H
