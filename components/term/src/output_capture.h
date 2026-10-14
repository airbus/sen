// === output_capture.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_OUTPUT_CAPTURE_H
#define SEN_COMPONENTS_TERM_SRC_OUTPUT_CAPTURE_H

// sen
// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"

// std
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

namespace sen::components::term
{

/// Captures stdout and stderr by redirecting the file descriptors through pipes.
/// Captured output is forwarded to a callback (typically the log pane).
///
/// Both, because a logger is not the only way a program writes: `printf`, `std::cout` and a spdlog
/// console sink all reach fd 1. Capturing the descriptor rather than the logger means nothing has to
/// be registered or configured for a component's output to appear in the pane.
///
/// Taking fd 1 means taking it from the renderer as well, since FTXUI writes its frames to
/// `std::cout` and ftxui 7.0.3 exposes no way to point it anywhere else. `CoutRouter` is what
/// separates the two, and the comment on it says how.
///
/// Windows keeps stderr only. The same split would work there, and nothing here has been run on
/// it, so it is left rather than written blind.
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
  /// Tells FTXUI the size of the terminal, which it cannot ask for once stdout is a pipe.
  ///
  /// ftxui 7.0.3 reads the size with `ioctl(STDOUT_FILENO, TIOCGWINSZ)` (screen/terminal.cpp:330),
  /// a descriptor number rather than the terminal, and capturing stdout puts a pipe there. The
  /// call then fails and FTXUI falls back to 80x24, which silently reshapes every pane. The saved
  /// descriptor is still the terminal, so the size is read from that and handed over through the
  /// fallback FTXUI already has. Called again on every drain so a resize is not missed.
  void publishTerminalSize() const;

public:
private:
  void readerThread(int pipeFd);

private:
  /// Queue one captured line, or count it as dropped if the queue is full. Call with `mutex_` held.
  void pushLineLocked(std::string line);

  OutputCallback callback_;
  std::atomic_bool running_ {true};

  int savedStderr_ = -1;

  int stderrPipeRead_ = -1;
  std::thread stderrReader_;

  int savedStdout_ = -1;
  int stdoutPipeRead_ = -1;
  std::thread stdoutReader_;

  /// Installed over `std::cout`'s own, and put back in the destructor.
  std::unique_ptr<std::streambuf> coutRouter_;
  std::streambuf* previousCoutBuffer_ = nullptr;

  // Bounded, and what it refuses is counted. A component that writes to stderr faster than the screen can
  // draw would otherwise grow this without limit, on a thread nothing throttles.
  std::mutex mutex_;
  std::vector<std::string> pendingLines_;
  std::size_t droppedLines_ = 0;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_OUTPUT_CAPTURE_H
