// === logger_impl.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/logmaster.stl.h"

// spdlog
#include <spdlog/logger.h>

namespace sen::components::logmaster
{

class LoggerImpl: public LoggerBase
{
  SEN_NOCOPY_NOMOVE(LoggerImpl)

public:
  LoggerImpl(const std::string& name, std::shared_ptr<spdlog::logger> logger);
  ~LoggerImpl() override;

public:
  [[nodiscard]] static ::spdlog::level::level_enum mapLogLevel(::sen::kernel::log::LogLevel level) noexcept;
  [[nodiscard]] static ::sen::kernel::log::LogLevel mapLogLevel(::spdlog::level::level_enum level) noexcept;

protected:
  void muteImpl() override;
  void unmuteImpl() override;
  void setLevelImpl(const ::sen::kernel::log::LogLevel& level) override;
  void setPatternImpl(const std::string& pattern) override;

private:
  std::shared_ptr<spdlog::logger> logger_;
  spdlog::level::level_enum level_;
};

}  // namespace sen::components::logmaster
