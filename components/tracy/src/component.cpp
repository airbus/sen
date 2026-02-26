// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/core/base/source_location.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/tracer.h"

// tracy
#include "client/TracyScoped.hpp"
#include "common/TracySystem.hpp"
#include "tracy/Tracy.hpp"

// std
#include <cstdint>
#include <list>
#include <string_view>
#include <tuple>

namespace
{

//--------------------------------------------------------------------------------------------------------------
// TracyTracer
//--------------------------------------------------------------------------------------------------------------

class TracyTracer final: public ::sen::kernel::Tracer
{
  SEN_NOCOPY_NOMOVE(TracyTracer)

public:
  explicit TracyTracer(std::string_view name): name_(name) { tracy::SetThreadName(name.data()); }

  ~TracyTracer() override = default;

public:
  void frameStart(std::string_view name) override
  {
    if (name.empty())
    {
      FrameMark;
    }
    else
    {
      FrameMarkStart(name.data());
    }
  }

  void frameEnd(std::string_view name) override
  {
    if (!name.empty())
    {
      FrameMarkEnd(name.data());
    }
  }

public:  // messages
  void message(std::string_view name) override { TracyMessageL(name.data()); }

  void plot(std::string_view name, float64_t value) override { TracyPlot(name.data(), value); }

  void plot(std::string_view name, int64_t value) override { TracyPlot(name.data(), value); }

protected:
  void zoneStart(std::string_view name, const sen::SourceLocation& location) override
  {
    zones_.emplace_back(location.lineNumber,
                        location.fileName.data(),
                        location.fileName.size(),
                        location.functionName.data(),
                        location.functionName.size(),
                        name.data(),
                        name.size(),
                        0,
                        -1,
                        true);
  }

  void zoneEnd() override { zones_.pop_back(); }

private:
  std::string name_;
  std::list<tracy::ScopedZone> zones_;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TracyComponent
//--------------------------------------------------------------------------------------------------------------

struct TracyComponent final: ::sen::kernel::Component
{
  [[nodiscard]] ::sen::kernel::FuncResult preload(::sen::kernel::PreloadApi&& api) override
  {
    ::tracy::StartupProfiler();  // NOLINT(misc-include-cleaner)
    api.installTracerFactory([](auto name) { return std::make_unique<TracyTracer>(name); });
    return ::sen::Ok();
  }

  [[nodiscard]] ::sen::kernel::FuncResult unload(::sen::kernel::UnloadApi&& api) override
  {
    std::ignore = api;
    ::tracy::ShutdownProfiler();  // NOLINT(misc-include-cleaner)
    return ::sen::Ok();
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }
};

SEN_COMPONENT(TracyComponent)
