// === shared_library.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_SHARED_LIBRARY_H
#define SEN_LIBS_KERNEL_SRC_SHARED_LIBRARY_H

namespace sen::kernel
{

/// Opaque container of an implementation-defined handle.
struct SharedLibrary
{
  void* nativeHandle = nullptr;  ///< A nullptr handle indicates an invalid library.
};

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_SHARED_LIBRARY_H
