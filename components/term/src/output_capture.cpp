// === output_capture.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "output_capture.h"

// component
#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/kernel/component_api.h"

// ftxui
#include <ftxui/screen/terminal.hpp>

// std
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#else
#  include <fcntl.h>
#  include <sys/ioctl.h>  // winsize and TIOCGWINSZ, for the size FTXUI cannot read through a pipe
#  include <sys/types.h>  // ssize_t, for the reader loop
#  include <unistd.h>
#endif

namespace sen::components::term
{

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

// Read-buffer size for the reader thread. Enough to drain most bursty writes in one syscall
// without holding a stale read for too long when traffic is light.
constexpr std::size_t readerBufferSize = 512U;

// Captured lines held for the render tick. Beyond this they are counted and dropped, and the count is
// reported, rather than growing a queue on a thread nothing throttles.
constexpr std::size_t maxPendingLines = 2000U;

// Longest line the reader will accumulate before handing it on. A component that writes a payload to
// stderr with no newline in it, a blob or a stack trace, would otherwise grow this buffer without bound.
// At the cap the line is passed on as it stands and accumulation starts again, so nothing is dropped and
// a very long line arrives split.
constexpr std::size_t maxCapturedLineLength = 4096U;

// Backoff when the pipe is temporarily empty (EAGAIN / EWOULDBLOCK). Small enough to feel
// responsive, large enough to avoid a busy spin.
constexpr std::chrono::milliseconds readerBackoff {50};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// OutputCapture
//--------------------------------------------------------------------------------------------------------------

void OutputCapture::publishTerminalSize() const
{
#ifndef _WIN32
  if (savedStdout_ < 0)
  {
    return;
  }

  winsize size {};
  if (::ioctl(savedStdout_, TIOCGWINSZ, &size) != 0 || size.ws_col == 0U || size.ws_row == 0U)
  {
    return;
  }

  ftxui::Terminal::SetFallbackSize({static_cast<int>(size.ws_col), static_cast<int>(size.ws_row)});
#endif
}

void OutputCapture::drain()
{
  publishTerminalSize();

  std::vector<std::string> lines;
  std::size_t dropped = 0;
  {
    std::lock_guard lock(mutex_);
    lines.swap(pendingLines_);
    dropped = std::exchange(droppedLines_, std::size_t {0});
  }

  for (const auto& line: lines)
  {
    callback_(line);
  }

  if (dropped > 0)
  {
    callback_(std::to_string(dropped) +
              " lines of captured output were dropped: they arrived faster than "
              "the screen can draw them.");
  }
}

void OutputCapture::pushLineLocked(std::string line)
{
  // Called with mutex_ held, from the reader thread. Dropping is the only bound available here:
  // blocking would stall whatever wrote to stderr, which is another component's thread.
  if (pendingLines_.size() >= maxPendingLines)
  {
    ++droppedLines_;
    return;
  }
  pendingLines_.push_back(std::move(line));
}

#ifndef _WIN32

namespace
{

/// Sends the renderer's own writes to the terminal and everyone else's into the capture pipe.
///
/// FTXUI writes each frame with `std::cout << buffer << std::flush` (ftxui 7.0.3, app.cpp:1120) and
/// offers no way to point it at another stream, so once fd 1 is a pipe the frames would be captured
/// along with the output and the screen would stay blank. The thread that installs the capture is
/// the thread that runs term's loop, and the loop is the only thing that renders, so the writing
/// thread is what tells a frame from a log line.
///
/// A write from any other thread goes to fd 1, which is the pipe, and is therefore captured. That
/// is the safe way round: a log line sent to the terminal by mistake is a line out of place, while
/// a frame sent to the pipe is a screen that never draws.
class CoutRouter final: public std::streambuf
{
public:
  CoutRouter(int terminalFd, std::thread::id renderer) noexcept: terminalFd_(terminalFd), renderer_(renderer) {}

protected:
  std::streamsize xsputn(const char* data, std::streamsize count) override
  {
    writeAll(data, static_cast<std::size_t>(count));
    return count;
  }

  int_type overflow(int_type character) override
  {
    if (character == traits_type::eof())
    {
      return traits_type::not_eof(character);
    }
    const auto byte = static_cast<char>(character);
    writeAll(&byte, 1U);
    return character;
  }

private:
  [[nodiscard]] int target() const noexcept
  {
    return std::this_thread::get_id() == renderer_ ? terminalFd_ : STDOUT_FILENO;
  }

  /// write() may take fewer bytes than offered, and a frame written short is a corrupted screen.
  static void writeAll(int descriptor, const char* data, std::size_t count) noexcept
  {
    std::size_t written = 0U;
    while (written < count)
    {
      const auto chunk = ::write(descriptor, data + written, count - written);
      if (chunk <= 0)
      {
        // Nothing useful to do with the error and nowhere to report it: this runs underneath
        // std::cout, and logging here would re-enter it.
        return;
      }
      written += static_cast<std::size_t>(chunk);
    }
  }

  void writeAll(const char* data, std::size_t count) const noexcept { writeAll(target(), data, count); }

  int terminalFd_;
  std::thread::id renderer_;
};

}  // namespace

OutputCapture::OutputCapture(OutputCallback callback): callback_(std::move(callback))
{
  // These warnings go through term's own named logger rather than spdlog's default one, so they are
  // attributed as `[term]` and can be levelled by name like any other component's. They still reach
  // the console rather than the output pane, and that is right: this runs before the capture is up, so
  // there is no pane yet to put them in.
  // F_DUPFD_CLOEXEC rather than dup(), so a process the clipboard spawns does not inherit the saved
  // descriptor. fcntl is variadic in POSIX and there is no non-variadic form of either request.
  savedStderr_ = ::fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 0);  // NOLINT(hicpp-vararg)
  if (savedStderr_ < 0)
  {
    getLogger()->warn("OutputCapture: duplicating stderr failed (errno={}); stderr will not be captured", errno);
    return;
  }

  std::array<int, 2> stderrPipe {};
  if (::pipe(stderrPipe.data()) != 0)
  {
    getLogger()->warn("OutputCapture: pipe() failed (errno={}); stderr will not be captured", errno);
    ::close(savedStderr_);
    savedStderr_ = -1;
    return;
  }

  stderrPipeRead_ = stderrPipe[0];

  if (::dup2(stderrPipe[1], STDERR_FILENO) < 0)
  {
    getLogger()->warn("OutputCapture: dup2() failed (errno={}); stderr will not be captured", errno);
    ::close(stderrPipe[1]);
    ::close(stderrPipeRead_);
    stderrPipeRead_ = -1;
    ::close(savedStderr_);
    savedStderr_ = -1;
    return;
  }
  ::close(stderrPipe[1]);

  ::fcntl(stderrPipeRead_, F_SETFL, O_NONBLOCK);  // NOLINT(hicpp-vararg) POSIX fcntl is variadic
  // Close-on-exec as well, for the same reason the saved descriptor is: a child the clipboard spawns
  // would otherwise inherit term's private pipe, and a long-lived one holds it open. F_SETFL above does
  // not touch this flag; FD_CLOEXEC lives in F_SETFD.
  ::fcntl(stderrPipeRead_, F_SETFD, FD_CLOEXEC);  // NOLINT(hicpp-vararg)

  // From here a crash banner goes to the real terminal rather than into the pipe below, which dies
  // with the process. Without this a fatal error printed nothing at all: no reason, and no path to
  // the report.
  kernel::KernelApi::setCrashBannerDescriptor(savedStderr_);

  stderrReader_ = std::thread([this]() { readerThread(stderrPipeRead_); });

  // stdout as well, which is where printf, std::cout and every spdlog console sink arrive. The
  // order matters: the renderer is moved onto the saved descriptor before fd 1 becomes a pipe, so
  // there is no window in which a frame could be written into the pipe and lost.
  savedStdout_ = ::fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 0);  // NOLINT(hicpp-vararg)
  if (savedStdout_ < 0)
  {
    getLogger()->warn("OutputCapture: duplicating stdout failed (errno={}); stdout will not be captured", errno);
    return;
  }

  coutRouter_ = std::make_unique<CoutRouter>(savedStdout_, std::this_thread::get_id());
  previousCoutBuffer_ = std::cout.rdbuf(coutRouter_.get());

  std::array<int, 2> stdoutPipe {};
  if (::pipe(stdoutPipe.data()) != 0)
  {
    getLogger()->warn("OutputCapture: pipe() failed (errno={}); stdout will not be captured", errno);
    std::cout.rdbuf(previousCoutBuffer_);
    previousCoutBuffer_ = nullptr;
    coutRouter_.reset();
    ::close(savedStdout_);
    savedStdout_ = -1;
    return;
  }

  stdoutPipeRead_ = stdoutPipe[0];

  if (::dup2(stdoutPipe[1], STDOUT_FILENO) < 0)
  {
    getLogger()->warn("OutputCapture: dup2() failed (errno={}); stdout will not be captured", errno);
    ::close(stdoutPipe[1]);
    ::close(stdoutPipeRead_);
    stdoutPipeRead_ = -1;
    std::cout.rdbuf(previousCoutBuffer_);
    previousCoutBuffer_ = nullptr;
    coutRouter_.reset();
    ::close(savedStdout_);
    savedStdout_ = -1;
    return;
  }
  ::close(stdoutPipe[1]);

  ::fcntl(stdoutPipeRead_, F_SETFL, O_NONBLOCK);  // NOLINT(hicpp-vararg)
  ::fcntl(stdoutPipeRead_, F_SETFD, FD_CLOEXEC);  // NOLINT(hicpp-vararg)

  stdoutReader_ = std::thread([this]() { readerThread(stdoutPipeRead_); });

  // Before the next frame rather than at the first drain: from the dup2 above, FTXUI's own query
  // reads a pipe and answers 80x24.
  publishTerminalSize();
}

OutputCapture::~OutputCapture()
{
  // Before the descriptor is closed: the kernel would otherwise hold a number that has been handed
  // back to the process and could name anything.
  kernel::KernelApi::setCrashBannerDescriptor(-1);

  running_ = false;

  // std::cout goes back to its own buffer before the descriptor moves, so nothing is writing
  // through the router while it is being taken apart.
  if (previousCoutBuffer_ != nullptr)
  {
    std::cout.rdbuf(previousCoutBuffer_);
    previousCoutBuffer_ = nullptr;
  }

  if (savedStdout_ >= 0)
  {
    ::dup2(savedStdout_, STDOUT_FILENO);
    ::close(savedStdout_);
  }

  if (savedStderr_ >= 0)
  {
    ::dup2(savedStderr_, STDERR_FILENO);
    ::close(savedStderr_);
  }

  // Join before closing the read end. The reader polls a non-blocking descriptor and leaves within one
  // backoff of `running_` going false, so it needs no wakeup. Closing a descriptor another thread is
  // reading from invites reuse: the number can be handed straight to another thread's open, and the
  // reader then reads an unrelated file into the output pane.
  if (stderrReader_.joinable())
  {
    stderrReader_.join();
  }

  if (stdoutReader_.joinable())
  {
    stdoutReader_.join();
  }

  // After both joins: the router holds the saved descriptor, and a reader still running would be
  // reading a number already handed back to the process.
  coutRouter_.reset();

  if (stderrPipeRead_ >= 0)
  {
    ::close(stderrPipeRead_);
  }

  if (stdoutPipeRead_ >= 0)
  {
    ::close(stdoutPipeRead_);
  }
}

void OutputCapture::readerThread(int pipeFd)
{
  // Created with std::thread, not through the kernel, so nothing else gives it the alternate signal
  // stack the crash handler needs. Without it a stack-overflow fault here produces no dump at all.
  kernel::KernelApi::prepareCurrentThreadForCrashReports();

  std::string lineBuffer;
  std::array<char, readerBufferSize> buf {};

  while (running_)
  {
    auto bytesRead = ::read(pipeFd, buf.data(), buf.size());

    if (bytesRead <= 0)
    {
      if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      {
        std::this_thread::sleep_for(readerBackoff);
        continue;
      }
      break;
    }

    for (ssize_t i = 0; i < bytesRead; ++i)
    {
      char c = buf.at(checkedConversion<std::size_t>(i));
      if (c == '\n')
      {
        std::lock_guard lock(mutex_);
        pushLineLocked(std::move(lineBuffer));
        lineBuffer.clear();
      }
      else if (c != '\r')
      {
        lineBuffer += c;
        if (lineBuffer.size() >= maxCapturedLineLength)
        {
          std::lock_guard lock(mutex_);
          pushLineLocked(std::move(lineBuffer));
          lineBuffer.clear();
        }
      }
    }
  }

  if (!lineBuffer.empty())
  {
    std::lock_guard lock(mutex_);
    pushLineLocked(std::move(lineBuffer));
  }
}

#else

OutputCapture::OutputCapture(OutputCallback callback): callback_(std::move(callback))
{
  savedStderr_ = ::_dup(_fileno(stderr));
  if (savedStderr_ < 0)
  {
    getLogger()->warn("OutputCapture: _dup(stderr) failed (errno={}); stderr will not be captured", errno);
    return;
  }

  std::array<int, 2> stderrPipe {};
  // _O_NOINHERIT matters as much as _O_BINARY: without it both pipe ends are inheritable, every child
  // gets a copy of the write end, and the EOF that retires the reader on Windows never arrives while one
  // of them lives. The POSIX branch gets the same from F_DUPFD_CLOEXEC.
  if (::_pipe(stderrPipe.data(), checkedConversion<unsigned int>(readerBufferSize), _O_BINARY | _O_NOINHERIT) != 0)
  {
    getLogger()->warn("OutputCapture: _pipe() failed (errno={}); stderr will not be captured", errno);
    ::_close(savedStderr_);
    savedStderr_ = -1;
    return;
  }

  stderrPipeRead_ = stderrPipe[0];

  if (::_dup2(stderrPipe[1], _fileno(stderr)) < 0)
  {
    getLogger()->warn("OutputCapture: _dup2() failed (errno={}); stderr will not be captured", errno);
    ::_close(stderrPipe[1]);
    ::_close(stderrPipeRead_);
    stderrPipeRead_ = -1;
    ::_close(savedStderr_);
    savedStderr_ = -1;
    return;
  }
  ::_close(stderrPipe[1]);

  // From here a crash banner goes to the real terminal rather than into the pipe below, which dies
  // with the process. Without this a fatal error printed nothing at all: no reason, and no path to
  // the report.
  kernel::KernelApi::setCrashBannerDescriptor(savedStderr_);

  stderrReader_ = std::thread([this]() { readerThread(stderrPipeRead_); });
}

OutputCapture::~OutputCapture()
{
  // Before the descriptor is closed: the kernel would otherwise hold a number that has been handed
  // back to the process and could name anything.
  kernel::KernelApi::setCrashBannerDescriptor(-1);

  running_ = false;

  if (savedStderr_ >= 0)
  {
    ::_dup2(savedStderr_, _fileno(stderr));
    ::_close(savedStderr_);
  }

  // Join before closing the read end, because closing a descriptor another thread is reading from
  // invites descriptor reuse: the number can be handed straight to another thread's open, and the reader
  // then reads an unrelated file into the output pane.
  //
  // What retires the reader here is EOF, not `running_`: `_pipe` has no non-blocking mode, so `::_read`
  // parks and `running_` is only tested between reads. The EOF comes from the `_dup2` above closing the
  // last handle on the write end, which is why the pipe is created with `_O_NOINHERIT` and why the
  // clipboard worker is stopped before this destructor runs. A child holding a copy of the write end
  // would mean no EOF and this join would never return.
  if (stderrReader_.joinable())
  {
    stderrReader_.join();
  }

  if (stderrPipeRead_ >= 0)
  {
    ::_close(stderrPipeRead_);
  }
}

void OutputCapture::readerThread(int pipeFd)
{
  // Created with std::thread, not through the kernel, so nothing else gives it the alternate signal
  // stack the crash handler needs. Without it a stack-overflow fault here produces no dump at all.
  kernel::KernelApi::prepareCurrentThreadForCrashReports();

  std::string lineBuffer;
  std::array<char, readerBufferSize> buf {};

  while (running_)
  {
    auto bytesRead = ::_read(pipeFd, buf.data(), checkedConversion<unsigned int>(buf.size()));

    if (bytesRead <= 0)
    {
      if (bytesRead < 0 && errno == EAGAIN)
      {
        std::this_thread::sleep_for(readerBackoff);
        continue;
      }
      break;
    }

    for (int i = 0; i < bytesRead; ++i)
    {
      char c = buf.at(checkedConversion<std::size_t>(i));
      if (c == '\n')
      {
        std::lock_guard lock(mutex_);
        pushLineLocked(std::move(lineBuffer));
        lineBuffer.clear();
      }
      else if (c != '\r')
      {
        lineBuffer += c;
        if (lineBuffer.size() >= maxCapturedLineLength)
        {
          std::lock_guard lock(mutex_);
          pushLineLocked(std::move(lineBuffer));
          lineBuffer.clear();
        }
      }
    }
  }

  if (!lineBuffer.empty())
  {
    std::lock_guard lock(mutex_);
    pushLineLocked(std::move(lineBuffer));
  }
}

#endif

}  // namespace sen::components::term
