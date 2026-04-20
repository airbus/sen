// === completer.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_COMPLETER_H
#define SEN_COMPONENTS_TERM_SRC_COMPLETER_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/object.h"

// std
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// forward declarations
namespace sen
{
class ClassType;
class CustomTypeRegistry;
}  // namespace sen

namespace sen::components::term
{

class LogRouter;
class ObjectStore;
class Scope;

/// The kind of completion candidate, used to determine behavior on selection.
enum class CompletionKind
{
  command,  /// A built-in command (cd, ls, etc.)
  path,     /// An intermediate path segment (needs '.' to continue)
  object,   /// A leaf object (can be followed by '.' for methods)
  method,   /// A method on an object
  value,    /// An argument value (log level, source name, query name, etc.)
};

/// A single completion candidate.
struct Completion
{
  std::string text;                             /// Replacement text for the token being completed.
  std::string display;                          /// Short annotation shown in the candidate grid.
  std::string detail = {};                      /// Longer description shown in the detail line when selected.
  CompletionKind kind = CompletionKind::value;  /// Determines behavior on selection.

  /// Number of method arguments. Used by the UI to decide between immediate execution and form.
  std::size_t argCount = 0U;
};

/// Result of a completion request.
struct CompletionResult
{
  std::vector<Completion> candidates;
  int replaceFrom = 0;  /// Start position of the token being replaced.
  int replaceTo = 0;    /// End position of the token being replaced (usually cursor pos).
};

/// Provides tab completion for the term input.
///
/// Maintains an in-memory index of objects, their methods, properties, and events, keyed by
/// relative name in the current scope. The index is updated incrementally via onObjectAdded/
/// onObjectRemoved callbacks; a full rebuild only happens when the scope changes.
class Completer final
{
  SEN_NOCOPY_NOMOVE(Completer)

public:
  Completer() = default;

  /// Refresh cached completion data if dirty. Called each update cycle.
  /// Full object rebuild only happens when markScopeDirty() was called; object add/remove
  /// events are handled incrementally via onObjectAdded/onObjectRemoved.
  void update(const Scope& scope, const ObjectStore& store, LogRouter& logRouter);

  /// Mark the object map dirty for a full rebuild on the next update(). Needed when the scope
  /// itself changes, since every cached relative name is now stale.
  void markScopeDirty();

  /// Mark the source/query lists dirty (call after open/close/query commands).
  void markListsDirty();

  /// Per-object add/remove hooks wired to ObjectStore's callbacks.
  /// onObjectAdded takes shared_ptr by value (ownership transfer); onObjectRemoved by const-ref.
  void onObjectAdded(const Scope& scope, std::shared_ptr<Object> obj);
  void onObjectRemoved(const Scope& scope, const std::shared_ptr<Object>& obj);

  /// Compute completions for the given input at the cursor position.
  [[nodiscard]] CompletionResult complete(std::string_view input, int cursorPos) const;

  /// Find an object by its relative name in the current scope.
  [[nodiscard]] std::shared_ptr<Object> findObject(std::string_view relativeName) const;

  /// Find the closest object names to `query` for "did you mean?" hints.
  [[nodiscard]] std::vector<std::string> findObjectSuggestions(std::string_view query,
                                                               std::size_t maxSuggestions = 3) const;

  /// Get the number of objects in the current scope (cached from last update).
  [[nodiscard]] std::size_t objectsInScopeCount() const;

  /// Set the type registry for type-name completion. Must be called before complete().
  void setTypeRegistry(const CustomTypeRegistry* registry);

  /// Set TUI mode. When false (REPL mode), TUI-only commands are excluded from completion.
  void setTuiMode(bool tuiMode) noexcept;

  /// Compute the longest common prefix among candidates.
  [[nodiscard]] static std::string commonPrefix(Span<const Completion> candidates);

private:
  [[nodiscard]] std::vector<Completion> completeCommand(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeObjectOrCommand(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeMethodName(std::string_view objectName,
                                                           std::string_view methodPrefix) const;
  [[nodiscard]] std::vector<Completion> completePropertyName(std::string_view objectName,
                                                             std::string_view propertyPrefix) const;
  [[nodiscard]] std::vector<Completion> completeEventName(std::string_view objectName,
                                                          std::string_view eventPrefix) const;
  [[nodiscard]] std::vector<Completion> completeWatchArg(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeListenArg(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeCdArg(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeOpenArg(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeCloseArg(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeQueryRmArg(std::string_view prefix) const;
  [[nodiscard]] std::vector<Completion> completeLogArg(std::string_view prefix,
                                                       Span<const std::string_view> tokens) const;

  [[nodiscard]] static std::vector<Completion> filterByPrefix(Span<const std::string> items,
                                                              std::string_view prefix,
                                                              std::string_view annotation = {});

  /// Parse "objectName.methodPrefix" from a token. Returns nullopt if no dot.
  [[nodiscard]] static std::optional<std::pair<std::string_view, std::string_view>> splitObjectMethod(
    std::string_view token);

  /// Get or build method completions for a ClassType (lazy, never invalidated).
  [[nodiscard]] const std::vector<Completion>& getMethodCompletions(ConstTypeHandle<ClassType> classType) const;

private:
  // Cached data
  std::vector<std::string> childNames_;        // immediate children of current scope (for cd/ls)
  std::vector<std::string> openSources_;       // currently open source names (for close)
  std::vector<std::string> availableSources_;  // discoverable source names (for open)
  std::vector<std::string> queryNames_;        // named query names (for cd @, query rm)
  std::vector<std::string> loggerNames_;       // spdlog logger names (for log level)

  // Object name -> Object for leaf objects in current scope
  std::unordered_map<std::string, std::shared_ptr<Object>> objectsByName_;

  // Refcount per first-segment child so we can maintain childNames_ incrementally:
  // count goes 0->1 -> insert into childNames_ (sorted), 1->0 -> erase.
  std::unordered_map<std::string, std::size_t> childCounts_;

  // ClassType -> method completions (lazy, never invalidated; class metadata is stable)
  mutable std::unordered_map<ConstTypeHandle<ClassType>, std::vector<Completion>> classMethodCache_;
  const CustomTypeRegistry* types_ = nullptr;
  bool tuiMode_ = true;

  // Cached scope depth for labeling path segments
  int scopeDepth_ = 0;              // 0=root, 1=session, 2=bus, 3+=group
  std::size_t objectsInScope_ = 0;  // count of objects in current scope

  // True when scope changed: next update() does a full object rebuild. Cleared afterwards.
  bool scopeDirty_ = true;
  bool listsDirty_ = true;
  uint64_t lastStoreGeneration_ = 0;

  // Test support
  friend class CompleterTestAccess;
  friend class CompleterIncrementalAccess;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_COMPLETER_H
