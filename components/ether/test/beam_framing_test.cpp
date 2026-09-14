// === beam_framing_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// ======================================================================================================================

// ether
#include "beamer.h"
#include "util.h"

// asio
#include <asio/buffer.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>   // NOLINT(misc-include-cleaner): declares asio::read, used below
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner): declares asio::write, used below

// gtest
#include <gtest/gtest.h>

// std
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace sen::components::ether
{
namespace
{

/// asio::read and asio::write live in the headers included above, but include-cleaner resolves them
/// to the impl headers those pull in. Wrapped so the exemption is stated once.
std::size_t readExactly(asio::ip::tcp::socket& socket, const asio::mutable_buffer& into)
{
  return asio::read(socket, into);  // NOLINT(misc-include-cleaner)
}

std::size_t writeAll(asio::ip::tcp::socket& socket, const asio::const_buffer& from)
{
  return asio::write(socket, from);  // NOLINT(misc-include-cleaner)
}

/// A connected pair of loopback sockets, so a test can control exactly how bytes arrive.
struct SocketPair
{
  asio::ip::tcp::acceptor acceptor;
  asio::ip::tcp::socket a;
  asio::ip::tcp::socket b;
};

SocketPair connectedPair(asio::io_context& io)
{
  SocketPair sockets {asio::ip::tcp::acceptor(io, {asio::ip::make_address("127.0.0.1"), 0}),
                      asio::ip::tcp::socket(io),
                      asio::ip::tcp::socket(io)};

  sockets.acceptor.async_accept(sockets.b, [](auto) {});
  sockets.a.connect(sockets.acceptor.local_endpoint());
  io.run();
  io.restart();
  return sockets;
}

std::vector<uint8_t> makeBeam(std::size_t size)
{
  std::vector<uint8_t> beam(size);
  std::iota(beam.begin(), beam.end(), uint8_t {1});
  return beam;
}

TEST(BeamFraming, TheHeaderRoundTrips)
{
  for (const std::size_t size: {std::size_t {1},
                                std::size_t {2},
                                std::size_t {255},
                                std::size_t {256},
                                std::size_t {65535},
                                BeamerBase::maxBeamSize})
  {
    EXPECT_EQ(decodeBeamHeader(encodeBeamHeader(size)), size) << "at size " << size;
  }
}

TEST(BeamFraming, ABeamSplitAcrossWritesIsReadBackWhole)
{
  // The failure this guards: a stream socket does not preserve message boundaries, so a beam can
  // arrive in pieces. Reading a beam per read then parses half of one and throws.
  asio::io_context io;
  auto sockets = connectedPair(io);

  const auto beam = makeBeam(200U);
  const auto header = encodeBeamHeader(beam.size());

  // Deliberately split in the middle of the payload, and again inside the header.
  writeAll(sockets.a, asio::buffer(header, 1U));
  writeAll(sockets.a, asio::buffer(header) + 1U);
  writeAll(sockets.a, asio::buffer(beam, 50U));
  writeAll(sockets.a, asio::buffer(beam) + 50U);

  std::array<uint8_t, beamHeaderSize> readHeader {};
  readExactly(sockets.b, asio::buffer(readHeader));
  const auto length = decodeBeamHeader(readHeader);
  ASSERT_EQ(length, beam.size());

  std::vector<uint8_t> readBeam(length);
  readExactly(sockets.b, asio::buffer(readBeam));
  EXPECT_EQ(readBeam, beam);
}

TEST(BeamFraming, TwoBeamsInOneArrivalAreReadSeparately)
{
  // The other half of the same failure: two beams can land in one read, and a reader taking one
  // beam per read would drop the second.
  asio::io_context io;
  auto sockets = connectedPair(io);

  const auto first = makeBeam(60U);
  const auto second = makeBeam(90U);

  std::vector<uint8_t> both;
  for (const auto& beam: {first, second})
  {
    const auto header = encodeBeamHeader(beam.size());
    both.insert(both.end(), header.begin(), header.end());
    both.insert(both.end(), beam.begin(), beam.end());
  }
  writeAll(sockets.a, asio::buffer(both));

  for (const auto& expected: {first, second})
  {
    std::array<uint8_t, beamHeaderSize> readHeader {};
    readExactly(sockets.b, asio::buffer(readHeader));
    std::vector<uint8_t> readBeam(decodeBeamHeader(readHeader));
    readExactly(sockets.b, asio::buffer(readBeam));
    EXPECT_EQ(readBeam, expected);
  }
}

TEST(BeamFraming, ASingleReadDoesNotSeeAWholeBeam)
{
  // The control, and the reason the two tests above are worth having: one receive returns whatever
  // has arrived, so reading a beam per receive parses part of one.
  asio::io_context io;
  auto sockets = connectedPair(io);

  const auto beam = makeBeam(200U);
  writeAll(sockets.a, asio::buffer(beam, 50U));

  std::vector<uint8_t> buffer(BeamerBase::maxBeamSize);
  const auto received = sockets.b.receive(asio::buffer(buffer));
  EXPECT_LT(received, beam.size()) << "a receive returned a whole beam; this control proves nothing";
}

}  // namespace
}  // namespace sen::components::ether
