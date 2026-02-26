// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "component.h"

// component
#include "base_router.h"
#include "http_server.h"
#include "sen_router.h"
#include "utils.h"

// sen
#include "sen/core/base/assert.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// asio
#include <asio/error_code.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>

// std
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace sen::components::rest
{

constexpr auto defaultRestAPIUpdateFreq = Duration::fromHertz(10.0);

kernel::FuncResult RestAPIComponent::preload(kernel::PreloadApi&& api)
{
  if (auto result = readConfig(api.getConfig()); result.isError())
  {
    return result;
  }

  return done();
}

kernel::FuncResult RestAPIComponent::unload(kernel::UnloadApi&& api)
{
  std::ignore = api;
  server_.reset();
  return done();
}

[[nodiscard]] kernel::FuncResult RestAPIComponent::run(kernel::RunApi& api)
{
  SEN_ASSERT(server_.has_value());

  std::unique_ptr<BaseRouter> router = std::make_unique<SenRouter>(api);

  asio::ip::tcp::endpoint endpoint(asio::ip::make_address_v4(config_.address), config_.port);
  server_.value().start(std::move(router),
                        endpoint,
                        threadPoolSize_,
                        [this]() { getLogger()->info("Listening on {}:{}", config_.address, config_.port); });

  auto result = api.execLoop(defaultRestAPIUpdateFreq, nullptr, false);
  server_.value().stop();

  return result;
}

[[nodiscard]] uint16_t RestAPIComponent::getThreadPoolSize() const { return threadPoolSize_; }

[[nodiscard]] uint16_t RestAPIComponent::getListenPort() const { return config_.port; }

[[nodiscard]] const std::string& RestAPIComponent::getListenAddress() const { return config_.address; }

sen::kernel::FuncResult RestAPIComponent::readConfig(const sen::VarMap& params)
{
  config_ = sen::toValue<Configuration>(params);

  // Validity check for server binding address
  asio::error_code ec;
  asio::ip::make_address_v4(config_.address, ec);
  if (ec)
  {
    return sen::Err(sen::kernel::ExecError {sen::kernel::ErrorCategory::expectationsNotMet, ec.message()});
  }
  // Set default thread pool size if optional has no value
  threadPoolSize_ = config_.threadPoolSize.value_or(defaultThreadPoolSize);

  return sen::kernel::done();
}

}  // namespace sen::components::rest

SEN_COMPONENT(sen::components::rest::RestAPIComponent)
