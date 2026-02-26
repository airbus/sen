// === posix_api.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./posix_api.h"

// system
#include <bits/pthreadtypes.h>
#include <bits/types/struct_sched_param.h>
#include <dlfcn.h>

// std
#include <pthread.h>
#include <sched.h>

#include <cstddef>

namespace sen::kernel
{

// The following class contains one-liners (single statements) that just wrap calls
// to the OS, with no other logic. As such, they do not require coverage: we are
// not testing the operating system.

// LCOV_EXCL_START

void* NativePosixAPI::dlopen(const char* file, int mode) noexcept { return ::dlopen(file, mode); }

int NativePosixAPI::dlclose(void* handle) noexcept { return ::dlclose(handle); }

void* NativePosixAPI::dlsym(void* handle, const char* name) noexcept { return ::dlsym(handle, name); }

int NativePosixAPI::pthread_attr_destroy(pthread_attr_t* attr) noexcept { return ::pthread_attr_destroy(attr); }

int NativePosixAPI::pthread_attr_init(pthread_attr_t* attr) noexcept { return ::pthread_attr_init(attr); }

int NativePosixAPI::pthread_create(pthread_t* pthread,
                                   const pthread_attr_t* attr,
                                   void* (*startRoutine)(void*),  // NOSONAR
                                   void* arg) noexcept
{
  return ::pthread_create(pthread, attr, startRoutine, arg);
}

int NativePosixAPI::pthread_attr_getschedpolicy(const pthread_attr_t* attr, int* policy) noexcept
{
  return ::pthread_attr_getschedpolicy(attr, policy);
}

int NativePosixAPI::sched_get_priority_max(int algorithm) noexcept { return ::sched_get_priority_max(algorithm); }

int NativePosixAPI::sched_get_priority_min(int algorithm) noexcept { return ::sched_get_priority_min(algorithm); }

int NativePosixAPI::pthread_attr_setstacksize(pthread_attr_t* attr, std::size_t stackSize) noexcept
{
  return ::pthread_attr_setstacksize(attr, stackSize);
}

int NativePosixAPI::pthread_attr_setschedparam(pthread_attr_t* attr, const sched_param* param) noexcept
{
  return ::pthread_attr_setschedparam(attr, param);
}

int NativePosixAPI::pthread_join(pthread_t thread, void** returnVal) noexcept
{
  return ::pthread_join(thread, returnVal);
}

int NativePosixAPI::pthread_cancel(pthread_t thread) noexcept { return ::pthread_cancel(thread); }

int NativePosixAPI::pthread_setcanceltype(int type, int* old_type) noexcept  // NOLINT
{
  return ::pthread_setcanceltype(type, old_type);
}

int NativePosixAPI::pthread_setaffinity_np(pthread_t thread, std::size_t cpuSetSize, const cpu_set_t* cpuSet) noexcept
{
  return ::pthread_setaffinity_np(thread, cpuSetSize, cpuSet);
}

int NativePosixAPI::pthread_setname_np(pthread_t thread, const char* name) noexcept
{
  return ::pthread_setname_np(thread, name);
}

int NativePosixAPI::pthread_detach(pthread_t thread) noexcept { return ::pthread_detach(thread); }

// LCOV_EXCL_STOP

}  // namespace sen::kernel
