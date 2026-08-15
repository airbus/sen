// === hash32_benchmark.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/hash32.h"

// 3rd party
#include <benchmark/benchmark.h>

// std
#include <cstdint>
#include <string>

namespace
{

void crc32String(benchmark::State& state)
{
  const std::string input(static_cast<std::size_t>(state.range(0)), 'x');

  for (auto _: state)
  {
    benchmark::DoNotOptimize(sen::crc32(input));
  }

  state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * state.range(0));
}

BENCHMARK(crc32String)->Range(8, 4096);

}  // namespace
