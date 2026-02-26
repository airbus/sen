// === beamer.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "beamer.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/util.h"

// generated code
#include "stl/discovery.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// asio
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>

// std
#include <algorithm>
#include <mutex>
#include <string>

namespace sen::components::ether
{

BeamerBase::BeamerBase(asio::io_context& io, ::sen::Duration beamPeriod): io_(io), beamPeriod_(beamPeriod), timer_(io_)
{
}

void BeamerBase::start(kernel::RunApi& api)
{
  timer_.expires_after(beamPeriod_.toChrono());
  timer_.async_wait([&api, this](const asio::error_code& errorCode) { beam(errorCode, api); });
}

void BeamerBase::startBeaming(const std::string& sessionName, const EnpointList& endpoints)
{
  const std::lock_guard lock(beamsMutex_);
  beams_.emplace_back();

  // set the beaming data
  auto& beam = beams_.back().beam;
  beam.info = sen::kernel::getOwnProcessInfo(sessionName);
  beam.beamPeriod = beamPeriod_;
  beam.protocolVersion = 2U;
  beam.endpoints = endpoints;

  // prepare the message
  auto& message = beams_.back().message;
  ResizableBufferWriter writer(message);
  OutputStream out(writer);
  SerializationTraits<SessionPresenceBeam>::write(out, beam);
  if (message.size() > maxBeamSize)
  {
    std::string err;
    err.append("session name '");
    err.append(sessionName);
    err.append("' is too long and cannot be beamed");
    throwRuntimeError(err);
  }
}

void BeamerBase::stopBeaming(const kernel::ProcessInfo& processInfo)
{
  auto isSame = [&](const BeamData& elem) -> bool { return elem.beam.info.sessionId == processInfo.sessionId; };

  const std::lock_guard lock(beamsMutex_);
  beams_.erase(std::remove_if(beams_.begin(), beams_.end(), isSame), beams_.end());
}

void BeamerBase::beam(const asio::error_code& timerError, kernel::RunApi& api)
{
  if (timerError)
  {
    throwRuntimeError(timerError.message());
  }

  if (api.stopRequested())
  {
    return;
  }

  {
    const std::lock_guard lock(beamsMutex_);
    for (const auto& beam: beams_)
    {
      sendBeam(beam.message);
    }
  }

  programNextBeam(api);
}

void BeamerBase::programNextBeam(kernel::RunApi& api)
{
  timer_.expires_at(timer_.expiry() + beamPeriod_.toChrono());
  timer_.async_wait([this, &api](const asio::error_code& timerError) { beam(timerError, api); });
}

}  // namespace sen::components::ether
