// === value_formatter.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_VALUE_FORMATTER_H
#define SEN_COMPONENTS_TERM_SRC_VALUE_FORMATTER_H

// sen
#include "sen/core/meta/var.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// forward declarations
namespace sen
{
class Type;
}

namespace sen::components::term
{

/// Format a Var value according to its Type as a styled FTXUI element.
/// Handles primitives, strings, enums, structs, sequences, variants, quantities, etc.
/// When compact is true, enum descriptions are suppressed and long strings are truncated.
[[nodiscard]] ftxui::Element formatValue(const Var& value, const Type& type, int indent = 0, bool compact = false);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_VALUE_FORMATTER_H
