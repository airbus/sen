// === log_emitter_impl.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_LOG_EMITTER_SRC_LOG_EMITTER_IMPL_H
#define SEN_TEST_UTIL_LOG_EMITTER_SRC_LOG_EMITTER_IMPL_H

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
  void emitStderr();
  void emitCout();
  void emitPrintf();
  void emitPlainSpdlog();

  /// Made through the kernel, so it carries whatever the kernel attaches at creation.
  std::shared_ptr<spdlog::logger> logger_;

  /// Made through spdlog's own factory, so the kernel never saw it made. Null unless the
  /// configured mode needs it: creating one changes what a registry sweep finds.
  std::shared_ptr<spdlog::logger> plainLogger_;
  uint64_t cycleCount_ = 0;
  uint64_t emitInterval_ = 30;  // cycles between emissions (depends on rate)
};

}  // namespace log_emitter

#endif  // SEN_TEST_UTIL_LOG_EMITTER_SRC_LOG_EMITTER_IMPL_H
