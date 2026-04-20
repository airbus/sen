// === command_engine.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_COMMAND_ENGINE_H
#define SEN_COMPONENTS_TERM_SRC_COMMAND_ENGINE_H

// local
#include "app.h"
#include "completer.h"
#include "log_router.h"
#include "object_store.h"
#include "scope.h"
#include "tree_view.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/event.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/term.stl.h"

// std
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace sen::components::term
{

/// Grouping for the "help" output.
enum class CommandCategory
{
  navigation,
  discovery,
  queries,
  logging,
  inspection,
  monitoring,
  general
};

/// Display name for a command category.
[[nodiscard]] std::string_view commandCategoryName(CommandCategory cat) noexcept;

/// Describes a built-in command: name, usage, help text, completion hint.
struct CommandDescriptor
{
  std::string_view name;
  CommandCategory category;
  std::string_view usage;           // shown by "help <cmd>"
  std::string_view detail;          // shown by "help <cmd>" (multi-line)
  std::string_view completionHint;  // shown in the Tab completion grid
  bool tuiOnly = false;             // true if the command requires TUI mode (watch pane, etc.)
};

/// Parses and dispatches user commands, wiring together the scope model,
/// object store, and the TUI app for output.
class CommandEngine final
{
  SEN_NOCOPY_NOMOVE(CommandEngine)

public:
  CommandEngine(const Configuration& config, kernel::RunApi& api, App& app, LogRouter& logRouter, Completer& completer);

  /// Process a single command string from the user.
  void execute(std::string_view input);

  /// Called each update cycle to flush notifications to the UI.
  void update();

  /// Get the current scope (for prompt/status bar updates).
  [[nodiscard]] const Scope& getScope() const noexcept;

  /// Get the object store.
  [[nodiscard]] const ObjectStore& getObjectStore() const noexcept;
  [[nodiscard]] ObjectStore& getObjectStore() noexcept;

  /// Called from the ObjectStore callbacks when objects appear or disappear.
  void onObjectAdded(const std::shared_ptr<Object>& obj);
  void onObjectRemoved(const std::shared_ptr<Object>& obj);

  /// Get the command descriptor table. Used by the completer for Tab completion.
  [[nodiscard]] static Span<const CommandDescriptor> getCommandDescriptors();

  /// Member-function pointer to a command handler.
  using Handler = void (CommandEngine::*)(std::string_view);

  /// Pairs a descriptor with its dispatch handler.
  struct CommandEntry
  {
    CommandDescriptor desc;
    Handler handler;
  };

  /// Get the static command table (descriptors + handlers). Defined in command_engine.cpp.
  [[nodiscard]] static Span<const CommandEntry> getCommandTable();

private:
  void cmdHelp(std::string_view args);
  void cmdCd(std::string_view args);
  void cmdPwd(std::string_view args);
  void cmdLs(std::string_view args);
  void cmdOpen(std::string_view args);
  void cmdClose(std::string_view args);
  void cmdQuery(std::string_view args);
  void cmdQueries(std::string_view args);
  void cmdLog(std::string_view args);
  void cmdWatch(std::string_view args);
  void cmdUnwatch(std::string_view args);
  void cmdWatches(std::string_view args);
  void cmdListen(std::string_view args);
  bool listenEvent(std::string_view objectName,
                   std::string_view eventName,
                   const std::shared_ptr<Object>& target,
                   const Event* event);
  void cmdUnlisten(std::string_view args);
  void cmdListeners(std::string_view args);
  void cmdInspect(std::string_view args);
  void cmdTypes(std::string_view args);
  void cmdUnits(std::string_view args);
  void cmdStatus(std::string_view args);
  void cmdVersion(std::string_view args);
  void cmdClear(std::string_view args);
  void cmdTheme(std::string_view args);
  void cmdShutdown(std::string_view args);

  /// Render a type's structure (struct fields, enum values, etc.) to the output pane.
  void inspectType(const Type& type);

  /// Try to resolve "object.method args" and invoke it or show its signature.
  /// For async method calls, uses a pending-call echo that updates on completion.
  /// Returns true if the input was handled as an object.method call.
  bool tryResolveObjectMethod(std::string_view input, std::string_view cmd, std::string_view args);

  bool watchProperty(std::string_view objectName,
                     std::string_view propertyName,
                     const std::shared_ptr<Object>& target,
                     const Property* property);

  void echoCommand(std::string_view input, bool isError = false);
  void reportError(std::string_view title, std::string_view message);
  void rebuildTreeIfNeeded();

  /// Handle the virtual "print" command that displays all properties of an object.
  void handlePrintCommand(std::string_view input, const std::shared_ptr<Object>& target, const ClassType& classType);

  /// Report an unknown method error with "did you mean?" suggestions.
  void reportUnknownMethod(std::string_view input,
                           std::string_view objectName,
                           std::string_view methodName,
                           const ClassType& classType);

  /// Invoke a method asynchronously with a pending spinner and result callback.
  void invokeMethodAsync(std::string_view input,
                         std::string_view methodName,
                         const std::shared_ptr<Object>& target,
                         const Method* method,
                         VarList argValues);

private:
  const Configuration& config_;
  kernel::RunApi& api_;
  App& app_;
  LogRouter& logRouter_;
  Completer& completer_;
  Scope scope_;
  ObjectStore store_;
  std::string hostName_;

  // Last time string sent to the status bar, used to avoid redundant refreshes
  std::string lastTimeStr_;
  // Last integer second (nanoseconds / 1e9) we formatted lastTimeStr_ for.
  // Re-formatting toLocalString/toUtcString 30x per second is wasteful; we
  // only need to rebuild when the second actually advances.
  int64_t lastTimeSeconds_ = -1;
  uint64_t lastStatusGeneration_ = 0;
  bool statusDirty_ = true;

  // Transport throughput tracking
  kernel::TransportStats prevTransportStats_ {};
  std::chrono::steady_clock::time_point prevTransportTime_ = std::chrono::steady_clock::now();
  std::string txRateStr_;
  std::string rxRateStr_;
  double txRate_ = 0.0;
  double rxRate_ = 0.0;

  // Cached object tree for ls
  TreeNode cachedTree_;
  uint64_t cachedTreeGeneration_ = 0;
  Scope::Kind cachedTreeScopeKind_ = Scope::Kind::root;
  std::string cachedTreeScopePath_;

  // Active property watches, keyed by "objectName.propertyName".
  struct Watch
  {
    std::string objectName;
    std::string propertyName;  // empty = "watch all properties" (expands on connect)
    std::weak_ptr<Object> object;
    const Property* property = nullptr;
    ConnectionGuard guard;
    bool pending = false;  // true = object hasn't appeared yet
  };
  std::map<std::string, Watch> watches_;

  struct Listener
  {
    std::string objectName;
    std::string eventName;
    std::weak_ptr<Object> object;
    const Event* event = nullptr;
    ConnectionGuard guard;
  };
  std::map<std::string, Listener> listeners_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_COMMAND_ENGINE_H
