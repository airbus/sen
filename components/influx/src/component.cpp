// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "database.h"
#include "recorder.h"
#include "tcp.h"
#include "udp.h"
#include "util.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/io/util.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>

// std
#include <memory>
#include <utility>

namespace sen::components::influx
{

class InfluxComponent: public kernel::Component
{
  SEN_NOCOPY_NOMOVE(InfluxComponent)

public:
  InfluxComponent() = default;
  ~InfluxComponent() override = default;

public:
  kernel::FuncResult run(kernel::RunApi& api) override
  {
    // read values from config
    auto config = toValue<Configuration>(api.getConfig());
    if (config.samplingPeriod.getNanoseconds() == 0)
    {
      constexpr auto defaultPeriod = Duration::fromHertz(60.0);
      config.samplingPeriod = defaultPeriod;
    }

    // warn if nothing to record
    if (config.selections.empty())
    {
      getLogger()->warn("no selections present; no data will be sent to Telegraf.");
      return done();
    }

    // create the transport
    asio::io_context io;
    std::unique_ptr<Transport> transport;
    switch (config.protocol)
    {
      case Protocol::udp:
        transport = std::make_unique<UDP>(io, config.telegrafAddress, config.telegrafPort);
        break;
      case Protocol::tcp:
        transport = std::make_unique<TCP>(io, config.telegrafAddress, config.telegrafPort);
        break;
    }

    // create the database
    auto database = std::make_unique<Database>(std::move(transport));
    if (config.batchSize != 0)
    {
      database->batchOf(config.batchSize);
    }

    // create the recorder
    auto recorder = std::make_unique<Recorder>(std::move(database), config.selections.asVector(), api);

    getLogger()->info("sending information to InfluxDB");
    return api.execLoop(config.samplingPeriod, nullptr, false);
  }
};

}  // namespace sen::components::influx

SEN_COMPONENT(sen::components::influx::InfluxComponent)
