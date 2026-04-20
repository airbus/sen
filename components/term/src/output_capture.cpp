// === output_capture.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "output_capture.h"

// sen
#include "sen/core/base/checked_conversions.h"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#else
#  include <fcntl.h>
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
  {
    std::lock_guard lock(mutex_);
    lines.swap(pendingLines_);
  }

  for (const auto& line: lines)
  {
    callback_(line);
  }
}

#ifndef _WIN32

OutputCapture::OutputCapture(OutputCallback callback): callback_(std::move(callback))
{
  savedStderr_ = ::dup(STDERR_FILENO);
  if (savedStderr_ < 0)
  {
    spdlog::warn("OutputCapture: dup(STDERR_FILENO) failed (errno={}); stderr will not be captured", errno);
    return;
  }

  std::array<int, 2> stderrPipe {};
  if (::pipe(stderrPipe.data()) != 0)
  {
    spdlog::warn("OutputCapture: pipe() failed (errno={}); stderr will not be captured", errno);
    ::close(savedStderr_);
    savedStderr_ = -1;
    return;
  }

  stderrPipeRead_ = stderrPipe[0];

  if (::dup2(stderrPipe[1], STDERR_FILENO) < 0)
  {
    spdlog::warn("OutputCapture: dup2() failed (errno={}); stderr will not be captured", errno);
    ::close(stderrPipe[1]);
    ::close(stderrPipeRead_);
    stderrPipeRead_ = -1;
    ::close(savedStderr_);
    savedStderr_ = -1;
    return;
  }
  ::close(stderrPipe[1]);

  ::fcntl(stderrPipeRead_, F_SETFL, O_NONBLOCK);

  stderrReader_ = std::thread([this]() { readerThread(stderrPipeRead_); });
}

OutputCapture::~OutputCapture()
{
  running_ = false;

  if (savedStderr_ >= 0)
  {
    ::dup2(savedStderr_, STDERR_FILENO);
    ::close(savedStderr_);
  }

  if (stderrPipeRead_ >= 0)
  {
    ::close(stderrPipeRead_);
  }

  if (stderrReader_.joinable())
  {
    stderrReader_.join();
  }
}

void OutputCapture::readerThread(int pipeFd)
{
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
      char c = buf[checkedConversion<std::size_t>(i)];
      if (c == '\n')
      {
        std::lock_guard lock(mutex_);
        pendingLines_.push_back(lineBuffer);
        lineBuffer.clear();
      }
      else if (c != '\r')
      {
        lineBuffer += c;
      }
    }
  }

  if (!lineBuffer.empty())
  {
    std::lock_guard lock(mutex_);
    pendingLines_.push_back(lineBuffer);
  }
}

#else

OutputCapture::OutputCapture(OutputCallback callback): callback_(std::move(callback))
{
  savedStderr_ = ::_dup(_fileno(stderr));
  if (savedStderr_ < 0)
  {
    spdlog::warn("OutputCapture: _dup(stderr) failed (errno={}); stderr will not be captured", errno);
    return;
  }

  std::array<int, 2> stderrPipe {};
  if (::_pipe(stderrPipe.data(), checkedConversion<unsigned int>(readerBufferSize), _O_BINARY) != 0)
  {
    spdlog::warn("OutputCapture: _pipe() failed (errno={}); stderr will not be captured", errno);
    ::_close(savedStderr_);
    savedStderr_ = -1;
    return;
  }

  stderrPipeRead_ = stderrPipe[0];

  if (::_dup2(stderrPipe[1], _fileno(stderr)) < 0)
  {
    spdlog::warn("OutputCapture: _dup2() failed (errno={}); stderr will not be captured", errno);
    ::_close(stderrPipe[1]);
    ::_close(stderrPipeRead_);
    stderrPipeRead_ = -1;
    ::_close(savedStderr_);
    savedStderr_ = -1;
    return;
  }
  ::_close(stderrPipe[1]);

  stderrReader_ = std::thread([this]() { readerThread(stderrPipeRead_); });
}

OutputCapture::~OutputCapture()
{
  running_ = false;

  if (savedStderr_ >= 0)
  {
    ::_dup2(savedStderr_, _fileno(stderr));
    ::_close(savedStderr_);
  }

  if (stderrPipeRead_ >= 0)
  {
    ::_close(stderrPipeRead_);
  }

  if (stderrReader_.joinable())
  {
    stderrReader_.join();
  }
}

void OutputCapture::readerThread(int pipeFd)
{
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
      char c = buf[checkedConversion<std::size_t>(i)];
      if (c == '\n')
      {
        std::lock_guard lock(mutex_);
        pendingLines_.push_back(lineBuffer);
        lineBuffer.clear();
      }
      else if (c != '\r')
      {
        lineBuffer += c;
      }
    }
  }

  if (!lineBuffer.empty())
  {
    std::lock_guard lock(mutex_);
    pendingLines_.push_back(lineBuffer);
  }
}

#endif

}  // namespace sen::components::term
