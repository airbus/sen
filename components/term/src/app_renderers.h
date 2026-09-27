// === app_renderers.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_APP_RENDERERS_H
#define SEN_COMPONENTS_TERM_SRC_APP_RENDERERS_H

// component
#include "arg_form.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Form rendering helpers
//--------------------------------------------------------------------------------------------------------------

/// Collect max name width for column alignment across the form field tree.
/// Widest field name in the tree, for the form's name column. `depth` bounds the recursion: the tree is
/// built from a peer's type and walked on every frame.
void collectFormColumnWidths(const ArgFormField& f, std::size_t& nameWidth, std::size_t depth = 0);

/// Build the contextual hint elements for a focused leaf (type name, description, format note).
ftxui::Elements focusedHintElements(const ArgFormField& leaf);

/// Find the enclosing quantityGroup (if any) for a given leaf and return its hint-relevant fields.
const ArgFormField& hintSourceFor(Span<const ArgFormField> topLevel, const ArgFormField* leaf);

/// Render a single field (leaf or composite group) into the output element list.
void renderFormField(const ArgFormField& f,
                     std::size_t depth,
                     std::size_t& leafCursor,
                     std::size_t focusedLeafIdx,
                     std::size_t nameWidth,
                     ftxui::Elements& out);

/// Render the complete guided-input form that replaces the input line while active.
ftxui::Element renderArgForm(const ArgForm& form);

/// Build the key-binding summary shown in the status bar while a form is active.
std::string formModeHint(const ArgForm& form);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_APP_RENDERERS_H
