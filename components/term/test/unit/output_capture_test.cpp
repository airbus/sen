// === output_capture_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "output_capture.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace sen::components::term
{
namespace
{

/// What the capture handed back, guarded because drain() runs on this thread while the reader
/// thread fills the queue behind it.
class Collected
{
public:
  void add(const std::string& line)
  {
    const std::lock_guard lock(mutex_);
    lines_.push_back(line);
  }

  [[nodiscard]] std::vector<std::string> lines() const
  {
    const std::lock_guard lock(mutex_);
    return lines_;
  }

private:
  mutable std::mutex mutex_;
  std::vector<std::string> lines_;
};

/// Drains until what the caller is waiting for has arrived, or the deadline passes. A fixed sleep
/// would either be too short on a loaded machine or waste the time on an idle one; the reader
/// thread is woken by the pipe, not by a clock.
template <typename Predicate>
void drainUntil(OutputCapture& capture, Predicate ready)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds {5};
  while (std::chrono::steady_clock::now() < deadline)
  {
    capture.drain();
    if (ready())
    {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds {10});
  }
  capture.drain();
}

/// Whether any captured line holds this text.
bool anyLineHolds(const Collected& collected, std::string_view text)
{
  const auto lines = collected.lines();
  return std::any_of(
    lines.begin(), lines.end(), [text](const std::string& line) { return line.find(text) != std::string::npos; });
}

/// @test
/// A line written to stderr reaches the callback, which is the whole point of the capture: a
/// component that writes to stderr rather than through spdlog is still seen in the log pane.
///
/// Every assertion is made after the capture is destroyed. While it lives, stderr is the pipe it
/// reads, so a failed assertion would be written into the thing under test and never printed.
TEST(OutputCapture, StderrReachesTheCallback)
{
  Collected collected;
  {
    OutputCapture capture([&collected](const std::string& line) { collected.add(line); });
    std::fputs("a component said something\n", stderr);
    std::fflush(stderr);
    drainUntil(capture, [&collected] { return !collected.lines().empty(); });
  }

  ASSERT_FALSE(collected.lines().empty()) << "nothing was captured from stderr";
  EXPECT_TRUE(anyLineHolds(collected, "a component said something"));
}

/// @test
/// Output that arrives faster than the screen can draw is dropped, and the drop is reported
/// rather than silent. A bounded queue that said nothing would make a component's own diagnostics
/// disappear exactly when it was being noisy, which is when they matter.
TEST(OutputCapture, DroppedOutputIsCountedAndReported)
{
  // Far above the 2000-line cap on purpose. The point is not the count but the pipe: six bytes a
  // line, this is more than a pipe buffer holds, so the writer below blocks until the reader has
  // taken some, and the reader cannot take that many without filling the queue and dropping.
  // A flood that fits in the pipe would race -- the first drain could empty the queue before the
  // reader ever reached the cap, and nothing would be dropped.
  constexpr std::size_t farOverTheCap = 20000U;
  Collected collected;
  {
    OutputCapture capture([&collected](const std::string& line) { collected.add(line); });
    for (std::size_t index = 0; index < farOverTheCap; ++index)
    {
      std::fputs("flood\n", stderr);
    }
    std::fflush(stderr);
    drainUntil(capture, [&collected] { return anyLineHolds(collected, "were dropped"); });
  }

  ASSERT_FALSE(collected.lines().empty());
  EXPECT_TRUE(anyLineHolds(collected, "lines of captured output were dropped"))
    << "the queue filled and said nothing about it";
}

}  // namespace
}  // namespace sen::components::term
