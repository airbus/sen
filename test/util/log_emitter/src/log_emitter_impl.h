// === log_emitter_impl.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef TEST_UTIL_LOG_EMITTER_SRC_LOG_EMITTER_IMPL_H
#define TEST_UTIL_LOG_EMITTER_SRC_LOG_EMITTER_IMPL_H

#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/log_emitter/log_emitter.stl.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <cstdint>
#include <memory>
#include <string>

namespace log_emitter
{

class EmitterImpl: public EmitterBase
{
  SEN_NOCOPY_NOMOVE(EmitterImpl)

public:
  EmitterImpl(const std::string& name, const sen::VarMap& args);
  ~EmitterImpl() override = default;

  void update(sen::kernel::RunApi& runApi) override;

private:
  void emitSpdlog();
  void emitStdout();

  std::shared_ptr<spdlog::logger> logger_;
  uint64_t cycleCount_ = 0;
  uint64_t emitInterval_ = 30;  // cycles between emissions (depends on rate)
};

}  // namespace log_emitter

#endif  // TEST_UTIL_LOG_EMITTER_SRC_LOG_EMITTER_IMPL_H
