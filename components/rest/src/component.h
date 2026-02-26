// === component.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_COMPONENT_H
#define SEN_COMPONENTS_REST_SRC_COMPONENT_H

// component
#include "http_server.h"

// generated code
#include "stl/configuration.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/var.h"

// kernel
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// std
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace sen::components::rest
{

/// Sen component that allows clients to manage Sen resources using a HTTP REST API
class RestAPIComponent: public kernel::Component
{
  SEN_NOCOPY_NOMOVE(RestAPIComponent)

public:
  RestAPIComponent() = default;
  ~RestAPIComponent() override = default;

  kernel::FuncResult preload(kernel::PreloadApi&& api) override;
  kernel::FuncResult unload(kernel::UnloadApi&& api) override;
  [[nodiscard]] kernel::FuncResult run(kernel::RunApi& api) override;
  [[nodiscard]] uint16_t getThreadPoolSize() const;
  [[nodiscard]] uint16_t getListenPort() const;
  [[nodiscard]] const std::string& getListenAddress() const;

private:
  sen::kernel::FuncResult readConfig(const sen::VarMap& params);
  static constexpr size_t defaultThreadPoolSize = 10;

private:
  uint16_t threadPoolSize_ {defaultThreadPoolSize};
  std::optional<HttpServer> server_ {std::in_place};
  Configuration config_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_COMPONENT_H
