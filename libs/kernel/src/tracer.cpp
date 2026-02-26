// === tracer.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/tracer.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/source_location.h"

// std
#include <cstdint>
#include <memory>
#include <string_view>

namespace sen::kernel
{

namespace
{

class DefaultTracer final: public Tracer
{
  SEN_NOCOPY_NOMOVE(DefaultTracer)

public:
  DefaultTracer() = default;
  ~DefaultTracer() override = default;

public:
  void frameStart(std::string_view /*name*/) override {}
  void frameEnd(std::string_view /*name*/) override {}
  void message(std::string_view /*name*/) override {}
  void plot(std::string_view /*name*/, float64_t /*value*/) override {}
  void plot(std::string_view /*name*/, int64_t /*value*/) override {}

protected:
  void zoneStart(std::string_view /*name*/, const SourceLocation& /*location*/) override {}
  void zoneEnd() override {}
};

}  // namespace

TracerFactory getDefaultTracerFactory()
{
  return [](auto /*name*/) { return std::make_unique<DefaultTracer>(); };
}

}  // namespace sen::kernel
