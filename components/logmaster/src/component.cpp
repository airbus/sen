// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// component
#include "logmaster_impl.h"

// sen
#include "sen/core/io/util.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/logmaster.stl.h"

// std
#include <chrono>
#include <memory>

namespace sen::components::logmaster
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr auto defaultTargetBus = "local.log";
constexpr auto defaultPeriod = std::chrono::milliseconds(500);

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// LogMasterComponent
//--------------------------------------------------------------------------------------------------------------

struct LogMasterComponent final: ::sen::kernel::Component
{
  [[nodiscard]] ::sen::kernel::FuncResult run(::sen::kernel::RunApi& api) override
  {
    auto config = ::sen::toValue<Config>(api.getConfig());
    if (config.targetBus.empty())
    {
      config.targetBus = defaultTargetBus;
    }

    if (config.period.getNanoseconds() == 0)
    {
      config.period = defaultPeriod;
    }

    auto targetBus = api.getSource(config.targetBus);
    auto master = std::make_shared<::sen::components::logmaster::LogMasterImpl>("master", config.targetBus);

    targetBus->add(master);

    return api.execLoop(config.period);
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }
};

}  // namespace sen::components::logmaster

SEN_COMPONENT(sen::components::logmaster::LogMasterComponent)
