// === tcp_beam_tracker.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_TCP_BEAM_TRACKER_H
#define SEN_COMPONENTS_ETHER_SRC_TCP_BEAM_TRACKER_H

#include "util.h"

// component
#include "beam_tracker.h"

// stl
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>

// std
#include <array>
#include <memory>
#include <string>

namespace sen::components::ether
{

class TcpBeamTracker final: public BeamTrackerBase, public std::enable_shared_from_this<TcpBeamTracker>
{
public:
  SEN_NOCOPY_NOMOVE(TcpBeamTracker)

public:
  using FoundCallback = std::function<void(const BeamInfo&)>;
  using LostCallback = std::function<void(const kernel::ProcessInfo&)>;

public:
  TcpBeamTracker(asio::io_context& io, Configuration config, FoundCallback&& onFound, LostCallback&& onLost);
  ~TcpBeamTracker() final;

public:
  void start(kernel::RunApi& api) final;
  asio::ip::tcp::socket& getSocket();
  void onConnected(std::function<void()>&& func);
  void onDisconnected(std::function<void()>&& func);

private:
  void connect();
  void receive();

  /// Reads the payload the header just announced, which is one whole beam and no more.
  void receiveBeam(std::size_t length);

  /// Closes the connection, tells anyone listening, and starts connecting again.
  ///
  /// Every way of losing the hub ends here, so a drop while reading a beam is handled the same as
  /// one while reading a header.
  void handleLostConnection(const std::string& reason);

private:
  asio::io_context& io_;
  Configuration config_;
  asio::ip::tcp::socket socket_;
  std::vector<SessionPresenceBeam> tracks_;
  asio::steady_timer timer_;
  std::vector<uint8_t> buffer_;
  std::array<uint8_t, beamHeaderSize> header_ {};
  kernel::RunApi* api_ = nullptr;
  asio::ip::tcp::endpoint endpoint_;
  std::function<void()> onConnected_;
  std::function<void()> onDisconnected_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_TCP_BEAM_TRACKER_H
