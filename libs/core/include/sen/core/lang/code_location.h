// === code_location.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_CODE_LOCATION_H
#define SEN_CORE_LANG_CODE_LOCATION_H

#include <cstdlib>

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Location of a character in a program.
struct CodeLocation
{
  const char* src;     /// Points to the source code
  std::size_t offset;  /// Offset relative to the start of the source code
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_CODE_LOCATION_H
