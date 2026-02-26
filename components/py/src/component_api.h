// === component_api.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_PY_SRC_COMPONENT_API_H
#define SEN_COMPONENTS_PY_SRC_COMPONENT_API_H

// component
#include "object_list_wrapper.h"
#include "object_source_wrapper.h"
#include "variant_conversion.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/package_manager.h"

// spdlog
#include <spdlog/logger.h>

// pybind
#include <pybind11/pytypes.h>

// std
#include <chrono>

namespace sen::components::py
{

/// The logger of this component
[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger();

class RunApi
{
  SEN_COPY_MOVE(RunApi)

public:
  RunApi(kernel::RunApi* runApi, Duration updatePeriod, kernel::PackageManager* packageManager);
  ~RunApi() = default;

public:
  void requestKernelStop(int exitCode);
  [[nodiscard]] PythonDuration getTime() const noexcept;
  [[nodiscard]] std::string getAppName() const noexcept;
  [[nodiscard]] pybind11::dict getConfig() const;
  [[nodiscard]] std::unique_ptr<ObjectSourceWrapper> getSource(const std::string& address);
  [[nodiscard]] std::unique_ptr<ObjectListWrapper> open(const std::string& selection);
  [[nodiscard]] ObjectWrapper make(const std::string& className,
                                   const std::string& instanceName,
                                   pybind11::kwargs args);
  [[nodiscard]] bool waitUntil(pybind11::object condition, std::chrono::nanoseconds timeout);
  [[nodiscard]] bool waitUntilCpp(std::function<bool()>&& condition, std::chrono::nanoseconds timeout = {});
  [[nodiscard]] bool getSyncCalls() const noexcept;
  [[nodiscard]] ::sen::impl::WorkQueue* getWorkQueue() noexcept;
  [[nodiscard]] std::chrono::nanoseconds getDefaultTimeout() const noexcept;
  void setDefaultTimeout(std::chrono::nanoseconds timeout) noexcept;
  void commit();

public:
  static void definePythonApi(pybind11::module_& pythonModule);

private:
  kernel::RunApi* runApi_;
  Duration updatePeriod_;
  kernel::PackageManager* packageManager_;
  bool syncCalls_ = false;
  std::chrono::nanoseconds defaultTimeout_;
};

}  // namespace sen::components::py

#endif  // SEN_COMPONENTS_PY_SRC_COMPONENT_API_H
