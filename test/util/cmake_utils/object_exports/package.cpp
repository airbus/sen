// === package.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "generated.h"
#ifdef _WIN32
#  define EXPORTED __declspec(dllexport)
#else
#  define EXPORTED __attribute__((visibility("default")))
#endif
EXPORTED int packageValue() { return generatedValue(); }
