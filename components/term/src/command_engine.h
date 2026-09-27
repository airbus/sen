// === command_engine.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_COMMAND_ENGINE_H
#define SEN_COMPONENTS_TERM_SRC_COMMAND_ENGINE_H

// component
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
#include <cstdint>
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
};

/// Parses and dispatches user commands, wiring together the scope model,
/// object store, and the TUI app for output.
class CommandEngine final
{
  SEN_NOCOPY_NOMOVE(CommandEngine)

public:
  CommandEngine(const Configuration& config, kernel::RunApi& api, App& app, LogRouter& logRouter, Completer& completer);
  ~CommandEngine() = default;

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

  /// Report a dotted command whose object half names nothing here, with a "did you mean" hint.
  void reportUnknownObject(std::string_view input, std::string_view objectName);

  /// Find `methodName` on `classType`, including the getters and setters that `getMethods()` leaves
  /// out. When the match is a setter, `setterProperty` is set to the property it belongs to so the
  /// caller can seed a form with the current value.
  [[nodiscard]] static const Method* findMethodOrAccessor(const ClassType& classType,
                                                          std::string_view methodName,
                                                          const Property*& setterProperty);

  /// Parse the typed arguments and adapt each to its declared type, reporting the first failure to the
  /// user. Returns false when something was reported and the caller should stop.
  bool parseAndAdaptArgs(std::string_view input,
                         std::string_view args,
                         const Method& method,
                         std::string_view signatureHint,
                         VarList& argValues);

  void echoCommand(std::string_view input, bool isError = false);
  /// Resolve and run one command line. Called only from execute(), which is the exception
  /// boundary: anything that throws out of here would terminate the kernel.
  void dispatch(std::string_view input, std::string_view cmd, std::string_view args);

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

  /// Render an answered call into its pending slot. Runs on the work queue, so it sits outside
  /// `execute`'s try; `invokeMethodAsync` wraps it.
  void finishMethodCall(std::size_t callId,
                        const std::string& inputStr,
                        const std::string& methodName,
                        const Type& returnType,
                        const std::string& timestamp,
                        const MethodResult<Var>& result);

  /// Answer a pending call with an error, if its slot is still open.
  /// Whether a call is still being waited for. Read it before touching anything the answer's type
  /// handle points at: the handle may be non-owning, and a late answer is the case where the object it
  /// came from may already be gone.
  [[nodiscard]] bool hasPendingCall(std::size_t callId) const;

  /// Finish a pending call as a failure. `title` separates a call that failed from a call that
  /// returned something term could not draw -- they used to read identically.
  void failPendingCall(std::size_t callId, const std::string& message, const std::string& title = "Call Error");

private:
  const Configuration& config_;
  kernel::RunApi& api_;
  App& app_;
  LogRouter& logRouter_;
  Completer& completer_;
  Scope scope_;
  ObjectStore store_;

  // Last time string sent to the status bar, used to avoid redundant refreshes
  // Re-formatting toLocalString/toUtcString 30x per second is wasteful; we
  // only need to rebuild when the second actually advances.

  // Transport throughput tracking

  // Cached object tree for ls
  TreeNode cachedTree_;
  uint64_t cachedTreeGeneration_ = 0;
  Scope::Kind cachedTreeScopeKind_ = Scope::Kind::root;
  std::string cachedTreeScopePath_;

  struct Listener
  {
    std::string objectName;  // as the user typed it, for display and for `unlisten`
    /// The object's full local name, captured when the listener was created. Reconnection matches
    /// on this rather than re-resolving the typed name, which is relative to whatever scope the
    /// user happens to be in when the object comes back.
    std::string fullName;
    std::string eventName;
    std::weak_ptr<Object> object;
    const Event* event = nullptr;
    ConnectionGuard guard;
  };
  std::map<std::string, Listener> listeners_;

  /// Calls that have been sent and not yet answered, with the moment each stops being worth
  /// waiting for. `callTimeout` was documented and never read, so a peer whose runner had stalled
  /// left the spinner turning and the screen redrawing at 30 Hz for as long as the term ran.
  struct PendingCall
  {
    std::string description;
    TimeStamp deadline;
  };
  std::map<std::size_t, PendingCall> pendingCalls_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_COMMAND_ENGINE_H
