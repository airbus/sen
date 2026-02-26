// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "replayer.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/io/util.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/components/replayer/configuration.stl.h"

// std
#include <memory>

namespace sen::components::replayer
{

class Component: public kernel::Component
{
  SEN_NOCOPY_NOMOVE(Component)

public:
  Component() = default;
  ~Component() override = default;

public:
  kernel::FuncResult run(kernel::RunApi& api) override
  {
    constexpr auto defaultPeriod = Duration::fromHertz(100.0);

    // read values from config
    auto config = toValue<Configuration>(api.getConfig());

    if (config.bus.empty())
    {
      config.bus = "local.replay";
    }
    if (config.samplingPeriod.get() == 0)
    {
      config.samplingPeriod = defaultPeriod;
    }

    auto controlBus = api.getSource(config.bus);
    auto replayer = std::make_shared<ReplayerImpl>("replayer", config.autoPlay, controlBus, api);
    controlBus->add(replayer);

    if (!config.autoOpen.empty())
    {
      replayer->open("replay", config.autoOpen);
    }

    return api.execLoop(defaultPeriod);
  }
};

}  // namespace sen::components::replayer

SEN_COMPONENT(sen::components::replayer::Component)
