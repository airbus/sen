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

// std
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
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
// stderr with no newline in it -- a blob, a stack trace -- used to grow this buffer without bound and
// then have it copied. At the cap the line is passed on as it stands and accumulation starts again,
// so nothing is dropped; a very long line arrives split.
constexpr std::size_t maxCapturedLineLength = 4096U;

// Backoff when the pipe is temporarily empty (EAGAIN / EWOULDBLOCK). Small enough to feel
// responsive, large enough to avoid a busy spin.
constexpr std::chrono::milliseconds readerBackoff {50};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// OutputCapture
//--------------------------------------------------------------------------------------------------------------

void OutputCapture::drain()
{
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

OutputCapture::OutputCapture(OutputCallback callback): callback_(std::move(callback))
{
  // These warnings go through term's own named logger rather than spdlog's default one, so they are
  // attributed as `[term]` and can be levelled by name like any other component's. They still reach
  // the console rather than the output pane, and that is right: this runs before LogRouter exists, so
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
}

OutputCapture::~OutputCapture()
{
  // Before the descriptor is closed: the kernel would otherwise hold a number that has been handed
  // back to the process and could name anything.
  kernel::KernelApi::setCrashBannerDescriptor(-1);

  running_ = false;

  if (savedStderr_ >= 0)
  {
    ::dup2(savedStderr_, STDERR_FILENO);
    ::close(savedStderr_);
  }

  // Join before closing the read end. The reader polls a non-blocking descriptor and leaves within one
  // backoff of `running_` going false, so it needs no wakeup -- and closing a descriptor another thread
  // is reading from invites descriptor reuse: the number can be handed straight to another thread's
  // open, and the reader then reads an unrelated file into the output pane.
  if (stderrReader_.joinable())
  {
    stderrReader_.join();
  }

  if (stderrPipeRead_ >= 0)
  {
    ::close(stderrPipeRead_);
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
  // gets a copy of the write end, and the destructor's EOF -- which is what actually retires the
  // reader on Windows -- never arrives while one of them lives. The POSIX twin gets this from
  // F_DUPFD_CLOEXEC, and said so in a comment a Windows reader would never look at.
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
  // What retires the reader here is EOF, not `running_`. `_pipe` has no non-blocking mode, so `::_read`
  // parks and `running_` is only tested between reads -- this arm carried the POSIX twin's comment
  // claiming a backoff poll, which it does not have. The EOF comes from the `_dup2` above closing the
  // last handle on the write end, which is why the pipe is created with `_O_NOINHERIT` and why the
  // clipboard worker is stopped before this destructor runs: a child holding a copy of the write end
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
