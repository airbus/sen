// === scope.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_SCOPE_H
#define SEN_COMPONENTS_TERM_SRC_SCOPE_H

// sen
#include "sen/core/base/compiler_macros.h"

// std
#include <string>
#include <string_view>

namespace sen::components::term
{

/// Represents the current navigation scope in the term component.
///
/// The scope is a hierarchical path through the Sen session/bus/group tree:
///   /                          root (all sessions visible)
///   /local                     session
///   /local.main                bus
///   /local.main/Sensors        group within a bus
///   /local.main/Sensors/GPS    specific object
///   @myquery                   named query scope
///
class Scope final
{
  SEN_NOCOPY_NOMOVE(Scope)

public:
  enum class Kind
  {
    root,     /// Top-level scope (all sessions visible)
    session,  /// Scoped to a session (e.g., "local")
    bus,      /// Scoped to a bus (e.g., "local.main")
    group,    /// Scoped to an object group within a bus
    query     /// Scoped to a named query
  };

  Scope() = default;

  /// Navigate to a target relative to the current scope.
  /// Handles "..", "/", "@queryname", "session.bus", and group/object names.
  /// Returns true if the navigation was valid (the caller must verify
  /// that the target actually exists).
  bool navigate(std::string_view target);

  /// Go up one level. Returns false if already at root.
  bool navigateUp();

  /// Swap to previous scope (cd -). Returns false if there is no previous scope.
  bool navigateBack();

  /// Get the current scope kind.
  [[nodiscard]] Kind getKind() const noexcept;

  /// Get the full path as a display string (e.g., "/local.main/Sensors").
  [[nodiscard]] std::string_view getPath() const noexcept;

  /// Get the session name (empty if at root).
  [[nodiscard]] std::string_view getSession() const noexcept;

  /// Get the bus name (empty if above bus level).
  [[nodiscard]] std::string_view getBus() const noexcept;

  /// Get the full bus address "session.bus" (empty if above bus level).
  [[nodiscard]] std::string getBusAddress() const;

  /// Get the group path within the bus (empty if at bus level or above).
  [[nodiscard]] std::string_view getGroupPath() const noexcept;

  /// Get the query name (empty if not a query scope).
  [[nodiscard]] std::string_view getQueryName() const noexcept;

  /// Generate the prompt string (e.g., "sen:/local.main> ").
  [[nodiscard]] std::string makePrompt() const;

  /// Check if a given object local name is within the current scope.
  /// The localName follows the format "component.session.bus[.rest...]".
  [[nodiscard]] bool contains(std::string_view objectLocalName) const;

  /// Strip the scope prefix from an object's local name to produce a relative name.
  [[nodiscard]] std::string relativeName(std::string_view objectLocalName) const;

private:
  void savePrevious();
  void rebuildPath();

private:
  Kind kind_ = Kind::root;
  std::string session_;
  std::string bus_;
  std::string groupPath_;
  std::string queryName_;
  std::string path_ = "/";

  // Previous scope for "cd -"
  Kind prevKind_ = Kind::root;
  std::string prevSession_;
  std::string prevBus_;
  std::string prevGroupPath_;
  std::string prevQueryName_;
  bool hasPrevious_ = false;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_SCOPE_H
