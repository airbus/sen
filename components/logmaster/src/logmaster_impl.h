// === logmaster_impl.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code

// generated code
#include "stl/logmaster.stl.h"

// component
#include "logger_impl.h"

// sen
#include "sen/kernel/component_api.h"

namespace sen::components::logmaster
{

class LogMasterImpl: public LogMasterBase
{
  SEN_NOCOPY_NOMOVE(LogMasterImpl)

public:
  using LogMasterBase::LogMasterBase;
  ~LogMasterImpl() override;

protected:
  void registered(kernel::RegistrationApi& api) override;
  void update(kernel::RunApi& runApi) override;

protected:
  void muteAllImpl() override;
  void unmuteAllImpl() override;
  void toggleMuteAllImpl() override;
  void setLevelImpl(const ::sen::kernel::log::LogLevel& level) override;

private:
  void checkForLoggers();

  template <typename F>
  void forAllLoggers(F&& func);

private:
  std::unordered_map<std::string, std::shared_ptr<LoggerImpl>> loggers_;
  std::shared_ptr<::sen::ObjectSource> targetBus_;
  std::size_t cyclesLeftForCheck_ = 10U;
  bool muted_ = false;
};

}  // namespace sen::components::logmaster
