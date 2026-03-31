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

HttpServer::~HttpServer() { getLogger()->trace("HttpServer destroyed"); }

void HttpServer::start(const asio::ip::tcp::endpoint& endpoint)
{
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
    accept(router_);
  }
  else
  {
    throwRuntimeError("rest cannot accept connections");
  }
}

void HttpServer::stop()
{
  getLogger()->trace("Stopping HttpServer");

  asio::error_code ec;
  std::ignore = acceptor_.close(ec);
  if (ec)
  {
    getLogger()->error("Error on acceptor::close: " + ec.message());
  }

  router_->releaseAll();
  context_.stop();

  getLogger()->trace("HttpServer stopped");
}

void HttpServer::accept(std::shared_ptr<BaseRouter> router)
{
  acceptor_.async_accept(
    [this, router](std::error_code ec, asio::ip::tcp::socket socket)
    {
      if (!ec)
      {
        auto session = std::make_shared<HttpSession>(context_, std::move(socket), router_);
        session->start();
        accept(router);
      }
    });
}

}  // namespace sen::components::rest
