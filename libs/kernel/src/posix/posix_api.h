// === posix_api.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_POSIX_POSIX_API_H_H
#define SEN_LIBS_KERNEL_SRC_POSIX_POSIX_API_H_H

#include "sen/core/base/class_helpers.h"

// os
#include <pthread.h>
#include <sched.h>

// std
#include <iosfwd>

#ifdef __APPLE__

// Definition of cpu_set for the Apple OS
typedef struct cpu_set  // NOLINT
{
  uint32_t count;
} cpu_set_t;  // NOLINT

inline void CPU_ZERO(cpu_set_t* cs) { cs->count = 0; }                                   // NOLINT
inline void CPU_SET(int num, cpu_set_t* cs) { cs->count |= (1 << num); }                 // NOLINT
inline int CPU_ISSET(int num, const cpu_set_t* cs) { return (cs->count & (1 << num)); }  // NOLINT

#endif

namespace sen::kernel
{

/// All calls we do to the Linux/Posix system. This class allows us to mock and
/// instrument system calls and evaluate our usage of the system.
class PosixAPI
{
public:
  SEN_NOCOPY_NOMOVE(PosixAPI)

public:  // defaults.
  PosixAPI() noexcept = default;
  virtual ~PosixAPI() noexcept = default;

public:
  [[nodiscard]] virtual void* dlopen(const char* file, int mode) noexcept = 0;                    // NOLINT NOSONAR
  [[nodiscard]] virtual int dlclose(void* handle) noexcept = 0;                                   // NOLINT NOSONAR
  [[nodiscard]] virtual void* dlsym(void* handle, const char* name) noexcept = 0;                 // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_attr_destroy(pthread_attr_t*) noexcept = 0;                   // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_attr_init(pthread_attr_t*) noexcept = 0;                      // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_create(pthread_t*,                                            // NOLINT NOSONAR
                                           const pthread_attr_t*,                                 // NOLINT NOSONAR
                                           void* (*)(void*),                                      // NOLINT NOSONAR
                                           void*) noexcept = 0;                                   // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_attr_getschedpolicy(const pthread_attr_t*,                    // NOLINT NOSONAR
                                                        int*) noexcept = 0;                       // NOLINT NOSONAR
  [[nodiscard]] virtual int sched_get_priority_max(int) noexcept = 0;                             // NOLINT NOSONAR
  [[nodiscard]] virtual int sched_get_priority_min(int) noexcept = 0;                             // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_attr_setstacksize(pthread_attr_t*,                            // NOLINT NOSONAR
                                                      std::size_t) noexcept = 0;                  // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_attr_setschedparam(pthread_attr_t*,                           // NOLINT NOSONAR
                                                       const sched_param*) noexcept = 0;          // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_join(pthread_t, void**) noexcept = 0;                         // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_cancel(pthread_t thread) noexcept = 0;                        // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_setcanceltype(int type, int* old_type) noexcept = 0;          // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_setaffinity_np(pthread_t,                                     // NOLINT NOSONAR
                                                   std::size_t,                                   // NOLINT NOSONAR
                                                   const cpu_set_t*) noexcept = 0;                // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_setname_np(pthread_t thread, const char* name) noexcept = 0;  // NOLINT NOSONAR
  [[nodiscard]] virtual int pthread_detach(pthread_t) noexcept = 0;                               // NOLINT NOSONAR
};

/// Implements real calls to the Linux API
class NativePosixAPI final: public PosixAPI
{
public:
  [[nodiscard]] void* dlopen(const char* file, int mode) noexcept override;                              // NOLINT
  [[nodiscard]] int dlclose(void* handle) noexcept override;                                             // NOLINT
  [[nodiscard]] void* dlsym(void* handle, const char* name) noexcept override;                           // NOLINT
  [[nodiscard]] int pthread_attr_destroy(pthread_attr_t*) noexcept override;                             // NOLINT
  [[nodiscard]] int pthread_attr_init(pthread_attr_t*) noexcept override;                                // NOLINT
  [[nodiscard]] int pthread_create(pthread_t*,                                                           // NOLINT
                                   const pthread_attr_t*,                                                // NOLINT
                                   void* (*)(void*),                                                     // NOLINT
                                   void*) noexcept override;                                             // NOLINT
  [[nodiscard]] int pthread_attr_getschedpolicy(const pthread_attr_t*, int*) noexcept override;          // NOLINT
  [[nodiscard]] int sched_get_priority_max(int) noexcept override;                                       // NOLINT
  [[nodiscard]] int sched_get_priority_min(int) noexcept override;                                       // NOLINT
  [[nodiscard]] int pthread_attr_setstacksize(pthread_attr_t*, std::size_t) noexcept override;           // NOLINT
  [[nodiscard]] int pthread_attr_setschedparam(pthread_attr_t*, const sched_param*) noexcept override;   // NOLINT
  [[nodiscard]] int pthread_join(pthread_t, void**) noexcept override;                                   // NOLINT
  [[nodiscard]] int pthread_cancel(pthread_t thread) noexcept override;                                  // NOLINT
  [[nodiscard]] int pthread_setcanceltype(int type, int* old_type) noexcept override;                    // NOLINT
  [[nodiscard]] int pthread_setaffinity_np(pthread_t, std::size_t, const cpu_set_t*) noexcept override;  // NOLINT
  [[nodiscard]] int pthread_setname_np(pthread_t thread, const char* name) noexcept override;            // NOLINT
  [[nodiscard]] int pthread_detach(pthread_t) noexcept override;                                         // NOLINT
};

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_POSIX_POSIX_API_H_H
