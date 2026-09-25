#pragma once

#include "stl/crash_demo/faulty.stl.h"

// sen
#include "sen/kernel/component_api.h"

// std
#include <cstdint>
#include <memory>

namespace crash_demo
{

/// Fails on purpose, after the configured number of cycles, so that there is something to open.
///
/// A fault and an escaping exception are answered differently. Using an address that is not mapped
/// is stopped by the operating system, and the crash handler writes a minidump from outside.
/// Letting an exception escape reaches Sen's own code first, so Sen writes a report as well.
class FaultyImpl: public FaultyBase
{
public:
  SEN_NOCOPY_NOMOVE(FaultyImpl)

  using FaultyBase::FaultyBase;
  ~FaultyImpl() override = default;

  void update(sen::kernel::RunApi& runApi) override;

private:
  std::shared_ptr<spdlog::logger> logger_;
  std::int32_t cycles_ = 0;
};

}  // namespace crash_demo
