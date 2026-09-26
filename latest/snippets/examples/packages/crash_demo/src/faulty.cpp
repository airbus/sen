#include "faulty.h"

// generated code
#include "stl/crash_demo/faulty.stl.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// std
#include <stdexcept>

namespace crash_demo
{

void FaultyImpl::update([[maybe_unused]] sen::kernel::RunApi& runApi)
{
  if (!logger_)
  {
    logger_ = sen::kernel::KernelApi::getOrCreateLogger("crash_demo");
  }

  if (cycles_ < getCyclesBeforeFailing())
  {
    ++cycles_;
    logger_->info("cycle {} of {}, still fine", cycles_, getCyclesBeforeFailing());
    return;
  }

  logger_->warn("about to fail on purpose, {}", getThrowInstead() ? "by throwing" : "by faulting");

  if (getThrowInstead())
  {
    throw std::runtime_error("the crash example was asked to fail by throwing");
  }

  // Deliberate: writing through a null pointer. Volatile so the compiler performs the write
  // rather than removing it as undefined behaviour.
  volatile int* nowhere = nullptr;
  *nowhere = 1;
}

SEN_EXPORT_CLASS(FaultyImpl)

}  // namespace crash_demo
