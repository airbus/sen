// === io_thread.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_IO_THREAD_H
#define SEN_COMPONENTS_INFLUX_SRC_IO_THREAD_H

// sen
#include "sen/core/base/compiler_macros.h"

// asio
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>

// std
#include <memory>
#include <thread>

namespace spdlog
{
// NOLINTNEXTLINE(readability-identifier-naming): spdlog's own class name
class logger;
}  // namespace spdlog

namespace sen::components::influx
{

/// Runs an io_context on a thread of its own, so an operation posted to it does not run on the
/// cycle doing the recording.
///
/// Declare the context above everything that holds a socket on it, and declare this below all of
/// them: until the destructor has joined, a handler may still be touching a socket and its buffers,
/// which die with the socket, and per-descriptor state, which lives as long as the context.
class IoThread
{
  SEN_NOCOPY_NOMOVE(IoThread)

public:
  explicit IoThread(asio::io_context& io);
  ~IoThread();

private:
  void pump() noexcept;

private:
  asio::io_context& io_;
  /// Declared above the thread, so it is held before run() is first entered. Without it run() finds
  /// no outstanding work and stops the context, either at once or as soon as a handler completes,
  /// and a stopped context runs nothing posted afterwards.
  asio::executor_work_guard<asio::io_context::executor_type> work_;
  /// Resolved before the thread starts. pump() is noexcept, and resolving a logger by name is a
  /// throwing call, which there would reach the std::terminate that path exists to avoid.
  std::shared_ptr<spdlog::logger> logger_;
  std::thread thread_;
};

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_IO_THREAD_H
