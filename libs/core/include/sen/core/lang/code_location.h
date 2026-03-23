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

/// Identifies the position of a lexeme within an STL source string.
/// Stored in every `StlToken` and used by the scanner/parser to produce precise error messages.
struct CodeLocation
{
  const char* src;     ///< Pointer to the start of the source string (not owned).
  std::size_t offset;  ///< Byte offset of the lexeme from the beginning of the source string.
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_CODE_LOCATION_H
