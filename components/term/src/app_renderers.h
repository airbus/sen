// === app_renderers.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_APP_RENDERERS_H
#define SEN_COMPONENTS_TERM_SRC_APP_RENDERERS_H

// local
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
// WatchEntry
//--------------------------------------------------------------------------------------------------------------

/// Visual state for a single entry in the watch pane. Defined here (rather than nested inside
/// App::UiState) so the watch-pane rendering helper can accept a map of these without depending
/// on the private UiState definition.
struct WatchEntry
{
  ftxui::Element value;
  bool inlineValue = true;  // true for scalars (render next to name); false -> below
  bool connected = true;    // false when the watched object has been removed
  bool pending = false;     // true when the object hasn't appeared yet
  uint8_t age = 0;          // starts at maxAge so the seed value flashes on watch
};

/// Flash-on-change animation length (frames). Shared between the renderer and UiState bookkeeping.
constexpr uint8_t watchMaxAge = 20U;

//--------------------------------------------------------------------------------------------------------------
// Form rendering helpers
//--------------------------------------------------------------------------------------------------------------

/// Collect max name width for column alignment across the form field tree.
void collectFormColumnWidths(const ArgFormField& f, std::size_t& nameWidth);

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

//--------------------------------------------------------------------------------------------------------------
// Watch pane rendering helper
//--------------------------------------------------------------------------------------------------------------

/// Build the row elements for the watch pane from the current watch entries. Groups watches by
/// object path (section headers) and renders tree connectors between properties within each group.
ftxui::Elements renderWatchEntries(const std::map<std::string, WatchEntry>& watches);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_APP_RENDERERS_H
