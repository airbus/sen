// === thread_impl.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./thread_impl.h"

#include "posix/../thread.h"
#include "posix/posix_api.h"

// sen
#include "sen/core/base/result.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// system
#ifdef __linux__
#  include <bits/types/struct_sched_param.h>
#endif

// other posix
#include <sched.h>

// std
#include <cstddef>
#include <memory>
#include <tuple>
#include <utility>

namespace sen::kernel
{

Result<ThreadImpl, ThreadCreateErr> ThreadImpl::make(const ThreadConfig& configuration,
                                                     std::shared_ptr<PosixAPI> api) noexcept
{
  ThreadImpl result(configuration, api);

  // POSIX.1 documents an ENOMEM error for pthread_attr_init()
  // but on Linux these functions always succeed.
  if (api->pthread_attr_init(&result.attributes_) != 0U)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  // configure priority
  {
    auto prioResult = result.configurePriority();
    if (prioResult.isError())
    {
      return Err(prioResult.getError());
    }
  }

  // configure stack size
  {
    auto configureStackResult = result.configureStackSize();
    if (configureStackResult.isError())
    {
      return Err(configureStackResult.getError());
    }
  }

  return Ok(result);
}

ThreadImpl::ThreadImpl(ThreadConfig configuration, std::shared_ptr<PosixAPI> api) noexcept
  : config_(std::move(configuration)), api_(std::move(api))
{
}

Result<void, ThreadCreateErr> ThreadImpl::run() noexcept
{
  // 'pthread_create' might fail with the following errors:
  // - EAGAIN Insufficient resources to create another thread.
  // - EINVAL Invalid settings in attr.
  // - EPERM  No permission to set the scheduling policy and parameters
  //          specified in attr.

  if (api_->pthread_create(&thread_, &attributes_, threadFunction, this) != 0U)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  // attributes are not needed anymore
  std::ignore = api_->pthread_attr_destroy(&attributes_);

  std::ignore = api_->pthread_setname_np(thread_, config_.name.c_str());

  // now that the thread is created, we have to set the affinity (if needed).
  return configureAffinity();
}

bool ThreadImpl::join() const noexcept
{
  // pthread_join can fail with:
  //
  // - EDEADLK: A deadlock was detected (e.g., two threads tried to join with
  //            each other); or thread specifies the calling thread.
  // - EINVAL:  thread is not a joinable thread.
  // - EINVAL:  Another thread is already waiting to join with this thread.
  // - ESRCH:   No thread with the ID thread could be found.

  return api_->pthread_join(thread_, nullptr) == 0;
}

bool ThreadImpl::kill() const noexcept
{
  // pthread_cancel can fail with:
  //
  // - ESRCH: No thread with the ID thread could be found.

  return api_->pthread_cancel(thread_) == 0;
}

Result<void, ThreadCreateErr> ThreadImpl::configurePriority() noexcept
{
  if (config_.priority == Priority::nominalMin)
  {
    return Ok();
  }

  // Fetch the scheduling policy by calling 'pthread_attr_getschedpolicy'.
  // This might return EINVAL if the value specified by attr does
  // not refer to an initialized thread attribute object. This cannot
  // happen, as this is already initialized in the configure()
  // function, which calls this one.
  int policy = 0;
  if (api_->pthread_attr_getschedpolicy(&attributes_, &policy) != 0U)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  const auto osMinimumPrio = api_->sched_get_priority_min(policy);
  const auto osMaximumPrio = api_->sched_get_priority_max(policy);
  const auto osNominalMin = 70;
  const auto osNominalMax = osMaximumPrio - 1;

  // 'sched_get_priority_max', and 'sched_get_priority_min' return -1 in case of error. The
  // error is EINVAL and means that the argument policy does not identify a defined scheduling
  // policy. This cannot happen, as 'policy' was just successfully fetched from the attributes.

  if (osMinimumPrio == -1)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  if (osMaximumPrio == -1)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  sched_param sched {};

  if (config_.priority == Priority::lowest)
  {
    sched.sched_priority = osMinimumPrio;
  }
  else if (config_.priority == Priority::highest)
  {
    sched.sched_priority = osMaximumPrio;
  }
  else
  {
    sched.sched_priority = (osNominalMax - osNominalMin) / 2;
  }

  if (api_->pthread_attr_setschedparam(&attributes_, &sched) != 0U)
  {
    return Err(ThreadCreateErr::internalOsError);
  }

  return Ok();
}

Result<void, ThreadCreateErr> ThreadImpl::configureStackSize() noexcept
{
  // if 0 we do not touch the default stack size.
  if (config_.stackSize == 0U)
  {
    return Ok();
  }

  // Set the stack size. The pthread_attr_setstacksize might fail with
  // EINVAL if the stack size is less than PTHREAD_STACK_MIN (16384) bytes.
  // On some systems, pthread_attr_setstacksize() can fail with the error
  // EINVAL if stacksize is not a multiple of the system page size.

  if (api_->pthread_attr_setstacksize(&attributes_, config_.stackSize) != 0U)
  {
    return Err(ThreadCreateErr::invalidStackSize);
  }

  return Ok();
}

bool ThreadImpl::detach() const noexcept { return api_->pthread_detach(thread_) == 0; }

Result<void, ThreadCreateErr> ThreadImpl::configureAffinity() const noexcept
{
  // 0 means no particular affinity
  if (config_.affinity == 0U)
  {
    return Ok();
  }

  cpu_set_t cpuSet {};
  CPU_ZERO(&cpuSet);

  auto mask = config_.affinity;
  for (std::size_t cpu = 0U; mask != 0U; cpu++, mask >>= 1U)
  {
    if ((mask & 1U) != 0U)
    {
      CPU_SET(cpu, &cpuSet);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic), LCOV_EXCL_LINE
    }
  }

  if (api_->pthread_setaffinity_np(thread_, sizeof(cpuSet), &cpuSet) != 0U)
  {
    return Err(ThreadCreateErr::invalidAffinity);
  }

  return Ok();
}

void* ThreadImpl::threadFunction(void* arg)
{
  auto* control = static_cast<ThreadImpl*>(arg);

  // pthread_setcanceltype can only fail with EINVAL if in case we provide invalid value for type.
  // std::ignore = control->api_->pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);

  control->config_.function(control->config_.arg);
  return nullptr;
}

}  // namespace sen::kernel
