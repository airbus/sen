// === io_thread.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "io_thread.h"

#include "util.h"

// asio
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>

// std
#include <exception>
#include <thread>

namespace sen::components::influx
{

IoThread::IoThread(asio::io_context& io)
  : io_(io), work_(asio::make_work_guard(io)), logger_(getLogger()), thread_([this] { pump(); })
{
}

IoThread::~IoThread()
{
  // stop() only asks: a handler already running carries on, and the join is what waits for it.
  // Work still queued is not run; it is destroyed later, when the context is.
  io_.stop();

  // Always joinable here, since the thread is built from a callable and this is neither copied nor
  // moved. Not wrapped in try/catch either: join() throws only on a self-join, and a destructor is
  // noexcept, so that reaches std::terminate. Detaching instead returns with a handler still
  // running, which is the quieter of the two.
  thread_.join();
}

void IoThread::pump() noexcept
{
  // Letting the exception out reaches std::terminate, which would end the application being
  // recorded over a failure to record it. A handler that threw has left the queue; an exception
  // out of the reactor task has not, and would be met again on every pass.
  while (true)
  {
    try
    {
      io_.run();
      return;
    }
    catch (const std::exception& error)
    {
      logger_->error("the sending thread caught an exception and carries on: {}", error.what());
    }
    catch (...)
    {
      logger_->error("the sending thread caught an exception of an unknown type and carries on");
    }
  }
}

}  // namespace sen::components::influx
