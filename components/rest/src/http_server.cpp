// === http_server.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "http_server.h"

// component
#include "base_router.h"
#include "http_session.h"
#include "utils.h"

// asio
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/socket_base.hpp>
#include <asio/system_error.hpp>
#include <asio/thread_pool.hpp>

// sen
#include "sen/core/base/assert.h"

// std
#include <cstdint>
#include <functional>
#include <memory>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// HttpServer
//--------------------------------------------------------------------------------------------------------------

HttpServer::HttpServer(): acceptor_(context_)
{
  // Left blank intentionally
}

HttpServer::~HttpServer() { stop(); }

void HttpServer::start(std::unique_ptr<BaseRouter> router,
                       const asio::ip::tcp::endpoint& endpoint,
                       uint16_t threadPoolSize,
                       std::function<void()> callback)
{
  threadPool_ = std::make_shared<asio::thread_pool>(threadPoolSize);

  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));

  asio::error_code ec;
  std::ignore = acceptor_.bind(endpoint, ec);
  if (ec)
  {
    getLogger()->error("Error on bind: " + ec.message());
    return;
  }

  std::ignore = acceptor_.listen(asio::socket_base::max_listen_connections, ec);
  if (!ec)
  {
    accept(std::move(router));
  }
  else
  {
    throwRuntimeError("rest cannot accept connections");
  }

  // Calling io_context::run() from only one thread means all event handlers execute in an implicit strand, due to
  // the io_context's guarantee that handlers are only invoked from inside run().
  // https://think-async.com/Asio/asio-1.30.2/doc/asio/overview/core/strands.html
  executor_ = std::thread(
    [this]()
    {
      try
      {
        context_.run();
      }
      catch (const asio::system_error& ec)
      {
        std::string err;
        {
          err.append("rest cannot start context. error: ");
          err.append(ec.what());
        }
        throwRuntimeError(err);
      }
    });

  callback();
}

void HttpServer::stop()
{
  SEN_ASSERT(threadPool_ != nullptr);

  asio::error_code ec;
  std::ignore = acceptor_.close(ec);
  if (ec)
  {
    getLogger()->error("Error on acceptor::close: " + ec.message());
  }

  context_.stop();
  threadPool_->join();
  if (executor_.joinable())
  {
    executor_.join();
  }
}

void HttpServer::accept(std::shared_ptr<BaseRouter> router)
{
  if (!threadPool_)
  {
    ::sen::throwRuntimeError("Thread pool not initialized");
  }

  acceptor_.async_accept(
    [this, router](std::error_code ec, asio::ip::tcp::socket socket)
    {
      if (!ec)
      {
        std::shared_ptr<HttpSession> session = std::make_shared<HttpSession>(threadPool_, std::move(socket), router);
        session->start();
        accept(router);
      }
    });
}

}  // namespace sen::components::rest
