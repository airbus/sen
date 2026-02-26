// === tcp_discovery_hub.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "tcp_discovery_hub.h"

#include "util.h"

// component
#include "beamer.h"

// sen
#include "sen/core/base/compiler_macros.h"

// stl
#include "stl/configuration.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/socket_base.hpp>

// std
#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

namespace sen::components::ether
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

class Client: public std::enable_shared_from_this<Client>
{
  SEN_NOCOPY_NOMOVE(Client)

public:
  Client(std::list<asio::ip::tcp::socket>* sockets, asio::ip::tcp::socket& socket): sockets_(sockets), socket_(socket)
  {
    buffer_.resize(BeamerBase::maxBeamSize);
  }

  ~Client() = default;

  void receive()
  {
    socket_.async_receive(asio::buffer(buffer_),
                          [us = shared_from_this(), ourSocketPtr = &socket_](auto ec, auto /*len*/)
                          {
                            if (!ec)
                            {
                              for (auto& sock: *us->sockets_)
                              {
                                if (&sock != ourSocketPtr)
                                {
                                  auto bufferCopy = std::make_shared<std::vector<uint8_t>>(us->buffer_);
                                  sock.async_send(asio::buffer(*bufferCopy),
                                                  [bufferCopy, us](auto ec, auto /*len*/)
                                                  {
                                                    if (ec)
                                                    {
                                                      us->disconnected();
                                                    }
                                                  });
                                }
                              }

                              us->receive();
                            }
                            else
                            {
                              us->disconnected();
                            }
                          });
  }

private:
  void disconnected()
  {
    sockets_->erase(
      std::find_if(sockets_->begin(), sockets_->end(), [this](auto& other) { return &socket_ == &other; }));
  }

private:
  std::list<asio::ip::tcp::socket>* sockets_;
  std::vector<uint8_t> buffer_;
  asio::ip::tcp::socket& socket_;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TcpDiscoveryHub
//--------------------------------------------------------------------------------------------------------------

TcpDiscoveryHub::TcpDiscoveryHub(asio::io_context& io, Configuration config): acceptor_(io), config_(std::move(config))
{
  asio::ip::tcp::endpoint endpoint(asio::ip::address_v4::any(), config_.runDiscoveryHub.value());
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
  doAccept();
}

TcpDiscoveryHub::~TcpDiscoveryHub()
{
  try
  {
    acceptor_.cancel();
    acceptor_.close();
  }
  catch (...)
  {
    // swallow error
  }

  for (auto& elem: sockets_)
  {
    try
    {
      asio::error_code ec;

      elem.shutdown(asio::socket_base::shutdown_both, ec);  // NOLINT(bugprone-unused-return-value)
      elem.close(ec);                                       // NOLINT(bugprone-unused-return-value)
    }
    catch (...)
    {
      // swallow error
    }
  }
}

void TcpDiscoveryHub::doAccept()
{
  acceptor_.async_accept(
    [this](std::error_code ec, asio::ip::tcp::socket socket)
    {
      if (!ec)
      {
        configureTcpSocket(socket, config_);
        sockets_.push_back(std::move(socket));

        auto client = std::make_shared<Client>(&sockets_, sockets_.back());
        client->receive();
      }
      doAccept();
    });
}

}  // namespace sen::components::ether
