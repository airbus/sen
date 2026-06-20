// === ws_client.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "ws_client.h"

// asio
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>        // NOLINT(misc-include-cleaner)
#include <asio/read_until.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/streambuf.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::jsonrpc::test
{

WsClient::WsClient(std::string host, uint16_t port): socket_(ctx_), host_(std::move(host)), port_(port) {}

void WsClient::connect(std::string_view path)
{
  asio::ip::tcp::resolver resolver(ctx_);
  auto endpoints = resolver.resolve(host_, std::to_string(port_));
  asio::connect(socket_, endpoints);  // NOLINT(misc-include-cleaner): asio's public header is included; the check maps
                                      // the symbol to an impl header

  std::ostringstream req;
  req << "GET " << path << " HTTP/1.1\r\n"
      << "Host: " << host_ << ":" << port_ << "\r\n"
      << "Upgrade: websocket\r\n"
      << "Connection: Upgrade\r\n"
      << "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      << "Sec-WebSocket-Version: 13\r\n\r\n";
  const auto reqStr = req.str();
  asio::write(socket_, asio::buffer(reqStr));  // NOLINT(misc-include-cleaner): asio's public header is included; the
                                               // check maps the symbol to an impl header

  asio::streambuf response;
  asio::read_until(socket_, response, "\r\n\r\n");  // NOLINT(misc-include-cleaner): asio's public header is included;
                                                    // the check maps the symbol to an impl header
  std::istream resStream(&response);
  std::string statusLine;
  std::getline(resStream, statusLine);
  if (statusLine.find("101") == std::string::npos)
  {
    throw std::runtime_error("WsClient: handshake failed: " + statusLine);
  }
}

namespace
{

void sendShortFrame(asio::ip::tcp::socket& socket, std::uint8_t opcode, std::string_view payload)
{
  if (payload.size() > 125U)
  {
    throw std::runtime_error("WsClient: payload too large for short frame");
  }
  // FIN=1, RSV=0, opcode in low 4 bits; MASK=1, len in low 7 bits.
  std::array<std::uint8_t, 6> header {
    static_cast<std::uint8_t>(0x80U | opcode),
    static_cast<std::uint8_t>(0x80U | payload.size()),
    0x12U,
    0x34U,
    0x56U,
    0x78U,
  };
  std::string masked(payload);
  for (std::size_t i = 0; i < masked.size(); ++i)
  {
    masked[i] = static_cast<char>(static_cast<std::uint8_t>(masked[i]) ^ header.at(2U + (i % 4U)));
  }
  asio::write(socket, asio::buffer(header));  // NOLINT(misc-include-cleaner): asio's public header is included; the
                                              // check maps the symbol to an impl header
  if (!masked.empty())
  {
    asio::write(socket, asio::buffer(masked));
  }
}

}  // namespace

void WsClient::sendText(std::string_view payload) { sendShortFrame(socket_, 0x1U, payload); }

void WsClient::sendBinary(std::string_view payload) { sendShortFrame(socket_, 0x2U, payload); }

std::string WsClient::receiveText()
{
  std::array<std::uint8_t, 2> header {};
  asio::read(socket_, asio::buffer(header));  // NOLINT(misc-include-cleaner): asio's public header is included; the
                                              // check maps the symbol to an impl header

  const auto opcode = static_cast<std::uint8_t>(header[0] & 0x0FU);
  if (opcode != 0x1U)
  {
    throw std::runtime_error("WsClient: expected text frame, got opcode " + std::to_string(opcode));
  }

  const bool masked = (header[1] & 0x80U) != 0U;
  std::uint64_t len = header[1] & 0x7FU;
  if (len == 126U)
  {
    std::array<std::uint8_t, 2> ext {};
    asio::read(socket_, asio::buffer(ext));
    len = (static_cast<std::uint64_t>(ext[0]) << 8U) | ext[1];
  }
  else if (len == 127U)
  {
    throw std::runtime_error("WsClient: 64-bit length not supported");
  }

  std::array<std::uint8_t, 4> mask {};
  if (masked)
  {
    asio::read(socket_, asio::buffer(mask));
  }

  std::string payload(static_cast<std::size_t>(len), '\0');
  if (len > 0U)
  {
    asio::read(socket_, asio::buffer(payload));
  }
  if (masked)
  {
    for (std::size_t i = 0; i < payload.size(); ++i)
    {
      payload[i] = static_cast<char>(static_cast<std::uint8_t>(payload[i]) ^ mask.at(i % 4U));
    }
  }

  return payload;
}

void WsClient::close()
{
  // FIN=1, opcode=0x8 (close), MASK=1, len=0.
  const std::array<std::uint8_t, 6> frame {0x88U, 0x80U, 0x00U, 0x00U, 0x00U, 0x00U};
  asio::error_code ec;
  std::ignore = asio::write(
    socket_,
    asio::buffer(frame),
    ec);  // NOLINT(misc-include-cleaner): asio's public header is included; the check maps the symbol to an impl header
  std::ignore = ec;
  std::ignore = socket_.close(ec);
  std::ignore = ec;
}

}  // namespace sen::components::jsonrpc::test
