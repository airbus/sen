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
#include <asio/write.hpp>

// std
#include <cstddef>
#include <cstdint>
#include <exception>
#include <list>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace sen::components::ether
{

namespace
{

/// Retrieves the remote endpoint name from a TCP socket.
std::string remoteEndpointName(asio::ip::tcp::socket& socket)
{
  asio::error_code ec;
  const auto endpoint = socket.remote_endpoint(ec);
  if (ec)
  {
    return std::string("<unknown: ").append(ec.message()).append(">");
  }

  return endpoint.address().to_string().append(":").append(std::to_string(endpoint.port()));
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Client
//--------------------------------------------------------------------------------------------------------------

/// Used by the TcpDiscoveryHub to relay messages to each of the processes connected to it
class TcpDiscoveryHub::Client: public std::enable_shared_from_this<TcpDiscoveryHub::Client>
{
  SEN_NOCOPY_NOMOVE(Client)

public:
  Client(TcpDiscoveryHub& hub, asio::ip::tcp::socket socket)
    : hub_(hub), peerName_(remoteEndpointName(socket)), socket_(std::move(socket))
  {
    buffer_.resize(BeamerBase::maxBeamSize);
  }

  ~Client() = default;

  void receive()
  {
    socket_.async_receive(
      asio::buffer(buffer_),
      [us = shared_from_this()](auto ec, auto length)
      {
        try
        {
          if (!ec)
          {
            us->hub_.broadcast(us, us->buffer_, length);
            us->receive();
          }
          else
          {
            getLogger()->info("discovery hub client {} read failed: {} ({})", us->peerName_, ec.message(), ec.value());
            us->hub_.disconnect(us);
          }
        }
        catch (const std::exception& error)
        {
          getLogger()->error("discovery hub client {} read handler failed: {}", us->peerName_, error.what());
          us->hub_.disconnect(us);
          throw;
        }
        catch (...)
        {
          getLogger()->error("discovery hub client {} read handler failed", us->peerName_);
          us->hub_.disconnect(us);
          throw;
        }
      });
  }

  void send(std::shared_ptr<std::vector<uint8_t>> buffer)
  {
    const auto data = asio::buffer(*buffer);
    asio::async_write(
      socket_,
      data,
      [buffer = std::move(buffer), us = shared_from_this()](auto ec, auto /*len*/)
      {
        (void)buffer;  // keep buffer alive until async_write completes
        try
        {
          if (ec)
          {
            getLogger()->info("discovery hub client {} write failed: {} ({})", us->peerName_, ec.message(), ec.value());
            us->hub_.disconnect(us);
          }
        }
        catch (const std::exception& error)
        {
          getLogger()->error("discovery hub client {} write handler failed: {}", us->peerName_, error.what());
          us->hub_.disconnect(us);
          throw;
        }
        catch (...)
        {
          getLogger()->error("discovery hub client {} write handler failed", us->peerName_);
          us->hub_.disconnect(us);
          throw;
        }
      });
  }

  void close()
  {
    asio::error_code ec;
    socket_.shutdown(asio::socket_base::shutdown_both, ec);  // NOLINT(bugprone-unused-return-value)
    socket_.close(ec);                                       // NOLINT(bugprone-unused-return-value)
  }

  [[nodiscard]] const std::string& peerName() const { return peerName_; }

private:
  TcpDiscoveryHub& hub_;
  std::string peerName_;
  std::vector<uint8_t> buffer_;
  asio::ip::tcp::socket socket_;
};

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
  getLogger()->info("discovery hub listening on {}:{}", endpoint.address().to_string(), endpoint.port());
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

  for (auto& elem: clients_)
  {
    elem->close();
  }
  clients_.clear();
}

void TcpDiscoveryHub::doAccept()
{
  acceptor_.async_accept(
    [this](std::error_code ec, asio::ip::tcp::socket socket)
    {
      try
      {
        if (!ec)
        {
          configureTcpSocket(socket, config_);

          auto client = std::make_shared<Client>(*this, std::move(socket));
          getLogger()->info("discovery hub accepted client {}", client->peerName());
          clients_.push_back(client);
          client->receive();
        }
        else
        {
          if (acceptor_.is_open())
          {
            getLogger()->info("discovery hub accept failed: {} ({})", ec.message(), ec.value());
          }
        }
      }
      catch (const std::exception& error)
      {
        getLogger()->error("discovery hub accept handler failed: {}", error.what());
        throw;
      }
      catch (...)
      {
        getLogger()->error("discovery hub accept handler failed");
        throw;
      }

      if (acceptor_.is_open())
      {
        doAccept();
      }
    });
}

void TcpDiscoveryHub::broadcast(const std::shared_ptr<Client>& sender,
                                const std::vector<uint8_t>& buffer,
                                std::size_t length)
{
  for (const auto& client: clients_)
  {
    if (client != sender)
    {
      try
      {
        client->send(
          std::make_shared<std::vector<uint8_t>>(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(length)));
      }
      catch (const std::exception& error)
      {
        getLogger()->error("discovery hub client {} write initiation failed: {}", client->peerName(), error.what());
        throw;
      }
      catch (...)
      {
        getLogger()->error("discovery hub client {} write initiation failed", client->peerName());
        throw;
      }
    }
  }
}

void TcpDiscoveryHub::disconnect(const std::shared_ptr<Client>& client)
{
  getLogger()->info("discovery hub disconnected client {}", client->peerName());
  client->close();
  clients_.remove(client);
}

}  // namespace sen::components::ether
