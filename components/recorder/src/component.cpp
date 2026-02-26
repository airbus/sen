// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "recorder.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/io/util.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/components/recorder/configuration.stl.h"
//
// std
#include <memory>
#include <vector>

namespace sen::components::recorder
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
    constexpr auto defaultPeriod = Duration::fromHertz(60.0);

    // read values from config
    auto config = toValue<Configuration>(api.getConfig());
    if (config.samplingPeriod.getNanoseconds() == 0)
    {
      config.samplingPeriod = defaultPeriod;
    }

    if (config.bus.empty())
    {
      config.bus = "local.recorder";
    }

    // create the recorders
    std::vector<std::shared_ptr<::recorder::RecorderImpl>> recorders;
    for (const auto& elem: config.recordings)
    {
      recorders.emplace_back(std::make_shared<::recorder::RecorderImpl>(elem, api));
    }

    // publish them in a local bus
    auto localSource = api.getSource(config.bus);
    localSource->add(recorders);

    return api.execLoop(config.samplingPeriod, nullptr, false);
  }
};

}  // namespace sen::components::recorder

SEN_COMPONENT(sen::components::recorder::Component)
