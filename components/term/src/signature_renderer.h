// === signature_renderer.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_SIGNATURE_RENDERER_H
#define SEN_COMPONENTS_TERM_SRC_SIGNATURE_RENDERER_H

// sen
#include "sen/core/meta/var.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <string_view>

// forward declarations
namespace sen
{
class Method;
class Property;
class Type;
}  // namespace sen

namespace sen::components::term
{

/// Render a manpage-style method signature as an FTXUI element.
[[nodiscard]] ftxui::Element renderMethodSignature(std::string_view objectName, const Method& method);

/// Render a property value display as an FTXUI element.
[[nodiscard]] ftxui::Element renderPropertyValue(const Property& prop, const Var& value);

/// Render a method call result as an FTXUI element.
[[nodiscard]] ftxui::Element renderMethodResult(const Var& value, const Type& returnType);

/// Render a styled error box as an FTXUI element.
[[nodiscard]] ftxui::Element renderError(std::string_view title, std::string_view message);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_SIGNATURE_RENDERER_H
