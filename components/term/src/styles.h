// === styles.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_STYLES_H
#define SEN_COMPONENTS_TERM_SRC_STYLES_H

// component
#include "completer.h"
#include "theme.h"

// ftxui
#include <ftxui/dom/elements.hpp>

namespace sen::components::term::styles
{

// All colors are read from the active theme so the entire UI adapts when the
// user selects a different theme (dark / light).

// Completion styles.

inline ftxui::Decorator completionCommand() { return ftxui::bold | ftxui::color(activeTheme().completionCommand); }
inline ftxui::Decorator completionPath() { return ftxui::color(activeTheme().info); }
inline ftxui::Decorator completionObject() { return ftxui::bold | ftxui::color(activeTheme().completionObject); }
inline ftxui::Decorator completionMethod() { return ftxui::color(activeTheme().completionCommand); }
inline ftxui::Decorator completionValue() { return ftxui::color(activeTheme().success); }

/// Return the style for a completion candidate based on its kind.
inline ftxui::Decorator completionStyle(CompletionKind kind)
{
  switch (kind)
  {
    case CompletionKind::command:
      return completionCommand();
    case CompletionKind::path:
      return completionPath();
    case CompletionKind::object:
      return completionObject();
    case CompletionKind::method:
      return completionMethod();
    case CompletionKind::value:
      return completionValue();
  }
  return ftxui::nothing;
}

// Value formatting styles.

inline ftxui::Decorator valueString() { return ftxui::color(activeTheme().valueString); }
inline ftxui::Decorator valueNumber() { return ftxui::color(activeTheme().valueNumber); }
inline ftxui::Decorator valueEnum() { return ftxui::color(activeTheme().valueNumber); }
inline ftxui::Decorator valueBool() { return ftxui::color(activeTheme().valueNumber); }
inline ftxui::Decorator valueEmpty() { return ftxui::color(activeTheme().mutedText); }
inline ftxui::Decorator listIndex() { return ftxui::color(activeTheme().valueNumber); }
inline ftxui::Decorator unitLabel() { return ftxui::color(activeTheme().mutedText); }

// Signature and type display styles.

inline ftxui::Decorator typeName() { return ftxui::color(activeTheme().info); }
inline ftxui::Decorator sectionTitle() { return ftxui::bold; }
inline ftxui::Decorator fieldName() { return ftxui::color(activeTheme().treeConnector); }
inline ftxui::Decorator descriptionText() { return ftxui::color(activeTheme().treeConnector); }

// Error styles.

inline ftxui::Decorator errorTitle() { return ftxui::bold | ftxui::color(activeTheme().error); }
inline ftxui::Decorator errorMessage() { return ftxui::color(activeTheme().error); }

// Input pane styles.

inline ftxui::Decorator inputBg() { return ftxui::bgcolor(activeTheme().inputBackground); }
inline ftxui::Decorator inputFg() { return ftxui::color(activeTheme().inputForeground); }
inline ftxui::Decorator completionBg() { return ftxui::bgcolor(activeTheme().completionBackground); }
inline ftxui::Decorator promptPath() { return ftxui::color(activeTheme().info); }
inline ftxui::Decorator promptSymbol() { return ftxui::color(activeTheme().accent) | ftxui::bold; }

// Banner styles.

inline ftxui::Color bannerQuoteColor() { return activeTheme().bannerQuote; }
inline ftxui::Color bannerAuthorColor() { return activeTheme().bannerAuthor; }
inline ftxui::Decorator bannerTitle() { return ftxui::color(activeTheme().bannerTitle); }
inline ftxui::Decorator bannerText() { return ftxui::color(activeTheme().bannerText); }

// Tree view styles.

inline ftxui::Color treeSession() { return activeTheme().treeSession; }
inline ftxui::Color treeBus() { return activeTheme().treeBus; }
inline ftxui::Color treeGroup() { return activeTheme().treeGroup; }
inline ftxui::Color treeObject() { return activeTheme().treeObject; }
inline ftxui::Color treePlain() { return activeTheme().treePlain; }
inline ftxui::Color treeConnector() { return activeTheme().treeConnector; }

// Command echo styles.

inline ftxui::Decorator echoSuccess() { return ftxui::color(activeTheme().success); }
inline ftxui::Decorator echoError() { return ftxui::color(activeTheme().error); }
inline ftxui::Decorator eventBadge() { return ftxui::color(activeTheme().accent) | ftxui::bold; }

// Semantic accent (reusable where inline Color::Yellow was used).

inline ftxui::Decorator accent() { return ftxui::color(activeTheme().accent); }

// Theme-aware muted text for de-emphasized labels, separators, and timestamps.
// Used in place of the terminal `dim` (SGR 2) attribute, which washes out on
// some palettes (notably light backgrounds).
inline ftxui::Decorator mutedText() { return ftxui::color(activeTheme().mutedText); }

// Log level styles (for log_router.cpp).

inline ftxui::Decorator logTrace() { return ftxui::color(activeTheme().logTrace); }
inline ftxui::Decorator logDebug() { return ftxui::color(activeTheme().info); }
inline ftxui::Decorator logInfo() { return ftxui::color(activeTheme().success); }
inline ftxui::Decorator logWarn() { return ftxui::color(activeTheme().accent) | ftxui::bold; }
inline ftxui::Decorator logError() { return ftxui::color(activeTheme().error) | ftxui::bold; }
inline ftxui::Decorator logCritical()
{
  return ftxui::color(activeTheme().logCriticalFg) | ftxui::bold | ftxui::bgcolor(activeTheme().logCriticalBg);
}

}  // namespace sen::components::term::styles

#endif  // SEN_COMPONENTS_TERM_SRC_STYLES_H
