// === command_engine.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "command_engine.h"

// component
#include "app.h"
#include "arg_form.h"
#include "completer.h"
#include "log_router.h"
#include "parse_utils.h"
#include "signature_renderer.h"
#include "styles.h"
#include "suggester.h"
#include "text_table.h"
#include "theme.h"
#include "tree_view.h"
#include "unicode.h"
#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/term.stl.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// spdlog
#include <spdlog/common.h>

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

namespace
{

/// Hidden replies to specific inputs. `fullInput` matches the whole input verbatim, before the
/// dispatcher, so a phrase can share its first word with a real command. `cmdToken` matches the
/// first word alone, after the dispatcher and object-method resolution, so a real command or
/// method is never shadowed.
struct Egg
{
  const char* cmdToken = nullptr;
  const char* fullInput = nullptr;
  const char* response = nullptr;
};

constexpr Egg eggs[] = {
  // Full-input phrases.
  {nullptr, "42", "42. But what was the question?"},
  {nullptr, "xyzzy", "Nothing happens."},

  // Command-token entries: muscle-memory slips from other tools, redirected to the real thing.

  // Windows-shell habits.
  {"dir", nullptr, "Not Windows. Use 'ls'."},
  {"cls", nullptr, "Not Windows. Use 'clear'."},

  // Unix-shell habits.
  {"man", nullptr, "No manpages here. Try 'help <command>'."},
  {"whoami", nullptr, "See 'status' for your session and scope."},
  {"cat", nullptr, "Try 'inspect <object>' to see its state."},
  {"grep", nullptr, "Try 'query' for SQL-filtered views, or Tab for completions."},
  {"ll", nullptr, "No bash aliases here. Use 'ls'."},
  {"la", nullptr, "No bash aliases here. Use 'ls'."},
  {"cd..", nullptr, "Use 'cd ..' with a space."},
  {"..", nullptr, "Use 'cd ..' to go up."},

  // Python / Node REPL habits.
  {"exit()", nullptr, "Python habits. Use 'exit'."},
  {"quit()", nullptr, "Same: try 'exit'."},
  {"quit", nullptr, "Use 'exit' or Ctrl+D."},

  // Vim habits.
  {":q", nullptr, "Not vim. Use 'exit' or Ctrl+D."},
  {":q!", nullptr, "Still not vim."},
  {":wq", nullptr, "Nothing to save. And still not vim."},
  {":help", nullptr, "No colons. Try 'help'."},

  // Typing an editor's name at the prompt.
  {"vim", nullptr, "No editor here. Try 'inspect <object>' for a read-only view."},
  {"vi", nullptr, "No editor here. Try 'inspect <object>' for a read-only view."},
  {"nano", nullptr, "No editor here. Try 'inspect <object>' for a read-only view."},
  {"emacs", nullptr, "No editor here. Try 'inspect <object>' for a read-only view."},

  // Typing another tool's name.
  {"git", nullptr, "Not a git shell. Try 'help'."},
  {"bash", nullptr, "You are already in a shell of sorts."},
  {"sh", nullptr, "You are already in a shell of sorts."},

  // Authority / destructive.
  {"sudo", nullptr, "This shell does not bow to sudo."},
  {"rm", nullptr, "No destruction here. Try 'query rm' or 'unlisten'."},

  // Friendly.
  {"hello", nullptr, "Hi. Type 'help' to get started."},
  {"hi", nullptr, "Hi. Type 'help' to get started."},
  {"thanks", nullptr, "Anytime."},
};

[[nodiscard]] const char* tryEggFullInput(std::string_view input) noexcept
{
  for (const auto& e: eggs)
  {
    if (e.fullInput != nullptr && input == e.fullInput)
    {
      return e.response;
    }
  }
  return nullptr;
}

[[nodiscard]] const char* tryEggCmdToken(std::string_view cmd) noexcept
{
  for (const auto& e: eggs)
  {
    if (e.cmdToken != nullptr && cmd == e.cmdToken)
    {
      return e.response;
    }
  }
  return nullptr;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// CommandCategory
//--------------------------------------------------------------------------------------------------------------

std::string_view commandCategoryName(CommandCategory cat) noexcept
{
  switch (cat)
  {
    case CommandCategory::navigation:
      return "Navigation";
    case CommandCategory::discovery:
      return "Discovery";
    case CommandCategory::queries:
      return "Queries";
    case CommandCategory::logging:
      return "Logging";
    case CommandCategory::inspection:
      return "Inspection";
    case CommandCategory::monitoring:
      return "Monitoring";
    case CommandCategory::general:
      return "General";
  }
  return "";
}

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

// clang-format off
Span<const CommandEngine::CommandEntry> CommandEngine::getCommandTable()
{
  static const CommandEntry table[] = {
    {{"cd",        CommandCategory::navigation,
                   "cd <path>",
                   "Change the current scope.\n"
                   "  cd local.demo     Navigate to a bus\n"
                   "  cd ..             Go up one level\n"
                   "  cd /              Go to root\n"
                   "  cd -              Go to previous scope\n"
                   "  cd @myquery       Navigate into a named query",
                   "change scope (.., /, -, @query, session.bus)"},
     &CommandEngine::cmdCd},
    {{"clear",     CommandCategory::general,
                   "clear",
                   "Clear everything the term has printed: results, log lines and event emissions.",
                   "clear the output pane"},
     &CommandEngine::cmdClear},
    {{"status",    CommandCategory::general,
                   "status",
                   "Show kernel status: run mode, components, and transport statistics.",
                   "show kernel runtime status"},
     &CommandEngine::cmdStatus},
    {{"close",     CommandCategory::discovery,
                   "close <source>",
                   "Close an open source.",
                   "close an open source"},
     &CommandEngine::cmdClose},
    {{"exit",      CommandCategory::general,
                   "exit | shutdown",
                   "Request kernel shutdown.",
                   "request kernel shutdown"},
     &CommandEngine::cmdShutdown},
    {{"help",      CommandCategory::general,
                   "help [command]",
                   "Show help for a specific command, or list all commands.",
                   "show available commands"},
     &CommandEngine::cmdHelp},
    {{"inspect",   CommandCategory::inspection,
                   "inspect <object | type>",
                   "Show the interface of an object or type.\n"
                   "Objects: properties (name, type, access), methods (signature), events.\n"
                   "Types: struct fields, enum values, variant alternatives, quantity bounds.",
                   "show the interface of an object or type"},
     &CommandEngine::cmdInspect},
    {{"listen",    CommandCategory::monitoring,
                   "listen <object>[.<event>]",
                   "Stream event emissions into the output, beside results and log lines.\n"
                   "  listen obj            Listen to all events\n"
                   "  listen obj.ev         Listen to one specific event\n"
                   "Listeners reconnect when objects reappear.",
                   "stream events from an object or a single event"},
     &CommandEngine::cmdListen},
    {{"listeners", CommandCategory::monitoring,
                   "listeners",
                   "List active event listeners.",
                   "list active event listeners"},
     &CommandEngine::cmdListeners},
    {{"log",       CommandCategory::logging,
                   "log [level <lvl>] [<name> <lvl>]",
                   "Show or control log levels.\n"
                   "  log                   Show all loggers and their levels\n"
                   "  log level info        Set global level\n"
                   "  log level mylog debug Set per-logger level",
                   "show or set log levels"},
     &CommandEngine::cmdLog},
    {{"ls",        CommandCategory::discovery,
                   "ls [path | @query]",
                   "List objects in the current scope, under a path, or matching a query.\n"
                   "  ls                All objects in scope\n"
                   "  ls local.demo     Only objects under local.demo\n"
                   "  ls @myquery       Objects matching a named query",
                   "list objects in current scope"},
     &CommandEngine::cmdLs},
    {{"open",      CommandCategory::discovery,
                   "open <source>",
                   "Open a source (session or session.bus) for object discovery.",
                   "open a source for discovery"},
     &CommandEngine::cmdOpen},
    {{"pwd",       CommandCategory::navigation,
                   "pwd",
                   "Print the current scope path.",
                   "print current scope path"},
     &CommandEngine::cmdPwd},
    {{"queries",   CommandCategory::queries,
                   "queries",
                   "List all named queries with their definitions.",
                   "list all named queries"},
     &CommandEngine::cmdQueries},
    {{"query",     CommandCategory::queries,
                   "query <name> <SELECT...> | query rm <name>",
                   "Create or remove a named query.\n"
                   "  query myq SELECT MyClass FROM local.demo\n"
                   "  query rm myq",
                   "<name> <SELECT...> | rm <name>"},
     &CommandEngine::cmdQuery},
    {{"shutdown",  CommandCategory::general,
                   "exit | shutdown",
                   "Request kernel shutdown.",
                   "request kernel shutdown"},
     &CommandEngine::cmdShutdown},
    {{"theme",     CommandCategory::general,
                   "theme <name>",
                   "Switch the color theme.\n"
                   "Available: oneDark, oneLight, catppuccinMocha, catppuccinLatte,\n"
                   "           dracula, nord, gruvboxDark, gruvboxLight, tokyoNight, solarizedLight",
                   "switch color theme"},
     &CommandEngine::cmdTheme},
    {{"types",     CommandCategory::inspection,
                   "types [filter]",
                   "List all registered types. A filter matches anywhere in the name, not just the start.\n"
                   "  types                 All types\n"
                   "  types term_showcase   Only types in that package",
                   "list registered types"},
     &CommandEngine::cmdTypes},
    {{"units",     CommandCategory::inspection,
                   "units [filter]",
                   "List all registered units. A filter matches anywhere in the category name.\n"
                   "  units                 All units\n"
                   "  units length          Only length units",
                   "list registered units"},
     &CommandEngine::cmdUnits},
    {{"version",   CommandCategory::general,
                   "version",
                   "Show version information for Sen, the kernel, and all loaded components.",
                   "show version information"},
     &CommandEngine::cmdVersion},
    {{"unlisten",  CommandCategory::monitoring,
                   "unlisten <object>[.<event>] | all",
                   "Stop listening to events.\n"
                   "  unlisten obj.ev       Stop one listener\n"
                   "  unlisten obj          Stop all listeners on an object\n"
                   "  unlisten all          Clear every listener",
                   "stop listening to an object or a single event"},
     &CommandEngine::cmdUnlisten},
  };
  // clang-format on
  return {table, std::size(table)};
}

namespace
{

/// Return built-in command names for "did you mean?" suggestions.
std::vector<std::string> builtInCommandNames()
{
  auto table = CommandEngine::getCommandTable();
  std::vector<std::string> names;
  names.reserve(table.size());
  for (const auto& e: table)
  {
    names.emplace_back(e.desc.name);
  }
  return names;
}

/// Peel alias/optional wrappers off a type, returning the innermost leaf.
const Type* peelToLeaf(const Type* t)
{
  if (t == nullptr)
  {
    return nullptr;
  }
  bool peeled = true;
  while (peeled)
  {
    peeled = false;
    if (auto* a = t->asAliasType(); a != nullptr)
    {
      t = a->getAliasedType().type();
      peeled = true;
    }
    if (auto* o = t->asOptionalType(); o != nullptr)
    {
      t = o->getType().type();
      peeled = true;
    }
  }
  return t;
}

/// Parse an argument string into a VarList using JSON semantics.
VarList parseArgs(const Method* method, std::string_view args)
{
  if (args.empty())
  {
    return {};
  }

  const auto& methodArgs = method->getArgs();

  // Single string argument: pass as-is without JSON quoting, and peel first, like the Duration path
  // below. Unpeeled, `obj.setName hello world` works for a `string` and the same method declared as
  // `alias string Name` or `optional<string>` takes the JSON path, splits into two tokens and fails.
  if (methodArgs.size() == 1 && args[0] != '"')
  {
    const auto* stringLeaf = peelToLeaf(methodArgs[0].type.type());
    if (stringLeaf != nullptr && stringLeaf->isStringType())
    {
      return {Var(std::string(args))};
    }
  }

  // Single Duration/TimeStamp argument: take the whole line as one value so unit suffixes work.
  if (methodArgs.size() == 1)
  {
    const auto* leaf = peelToLeaf(methodArgs[0].type.type());
    const bool timeValued = leaf != nullptr && (leaf->isDurationType() || leaf->isTimestampType());
    if (timeValued && args[0] != '"' && args[0] != '{' && args[0] != '[')
    {
      return {Var(std::string(args))};
    }
  }

  // Split on top-level whitespace/commas, rejoin as a JSON array.
  auto tokens = splitTopLevelArgs(args);
  std::string doc = R"({ "args": [)";
  for (std::size_t i = 0; i < tokens.size(); ++i)
  {
    if (i > 0)
    {
      doc += ", ";
    }
    doc.append(tokens[i]);
  }
  doc.append("] }");

  return findElement(fromJson(doc).get<VarMap>(), "args", "argument parsing error").get<VarList>();
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Construction
//--------------------------------------------------------------------------------------------------------------

CommandEngine::CommandEngine(const Configuration& config,
                             kernel::RunApi& api,
                             App& app,
                             LogRouter& logRouter,
                             Completer& completer)
  : config_(config), api_(api), app_(app), logRouter_(logRouter), completer_(completer), store_(api)
{
  for (const auto& source: config_.open)
  {
    if (auto err = store_.openSource(source); err.isError())
    {
      app_.appendInfo("Failed to open '" + source + "': " + err.getError());
    }
  }

  for (const auto& q: config_.query)
  {
    if (auto err = store_.createQuery(q.name, q.selection); err.isError())
    {
      app_.appendInfo("Failed to create query '" + q.name + "': " + err.getError());
    }
  }

  // After `open` and `query`, so a listener named on a source this configuration opens can resolve
  // immediately; one that cannot becomes a deferred listener and binds when the object appears.
  for (const auto& target: config_.listen)
  {
    cmdListen(target);
  }

  if (!config_.initialScope.empty())
  {
    scope_.navigate(config_.initialScope);
  }

  app_.setPrompt(scope_.makePrompt());
}

//--------------------------------------------------------------------------------------------------------------
// Execute
//--------------------------------------------------------------------------------------------------------------

void CommandEngine::execute(std::string_view input)
{
  if (input.empty())
  {
    return;
  }

  const bool formBefore = app_.hasActiveForm();

  auto [cmd, args] = splitCommand(input);

  // The exception boundary for a typed command. Nothing upstream catches: runner.cpp's
  // EXCEPTION_WRAP_BLOCK expands to a bare do/while, so a throw that escapes here reaches
  // std::terminate and takes every other component in the process with it. Answers arriving on the
  // work queue come in outside this try and are wrapped where they are handled.
  try
  {
    dispatch(input, cmd, args);
  }
  // Everything dispatch can throw arrives here, including transport and query-compiler failures, so the
  // title names the command: without it the user is left with whatever internal text the exception
  // carried and no clue which command produced it.
  catch (const std::exception& e)
  {
    reportError("'" + std::string(cmd) + "' failed",
                std::string(e.what()) +
                  "\nThis is an internal error: the "
                  "message above comes from inside Sen, "
                  "not from your input.");
  }
  catch (...)
  {
    reportError("'" + std::string(cmd) + "' failed", "it threw an exception carrying no message.");
  }

  // Form's re-entrant execute() adds its own separator.
  if (!formBefore && app_.hasActiveForm())
  {
    return;
  }

  app_.appendOutput("");
  completer_.markListsDirty();
}

void CommandEngine::dispatch(std::string_view input, std::string_view cmd, std::string_view args)
{
  const char* fullEgg = tryEggFullInput(input);

  if (fullEgg != nullptr)
  {
    echoCommand(input);
    app_.appendInfo(fullEgg);
  }
  else if (cmd == "?")
  {
    echoCommand(input);
    cmdHelp(args);
  }
  else
  {
    const CommandEntry* entry = nullptr;
    for (const auto& e: getCommandTable())
    {
      if (e.desc.name == cmd)
      {
        entry = &e;
        break;
      }
    }

    if (entry != nullptr)
    {
      echoCommand(input);
      (this->*entry->handler)(args);
    }
    else if (!tryResolveObjectMethod(input, cmd, args))
    {
      if (const auto* egg = tryEggCmdToken(cmd); egg != nullptr)
      {
        echoCommand(input);
        app_.appendInfo(egg);
      }
      else
      {
        echoCommand(input, /*isError=*/true);

        // Accepting a path or object candidate appends a dot, so this exact line is one Tab away and the
        // user did not type the dot themselves. Saying "not a recognized command" about it is true and
        // useless.
        if (cmd.size() > 1U && cmd.back() == '.' && completer_.findObject(cmd.substr(0, cmd.size() - 1U)))
        {
          reportError("Incomplete Command",
                      "'" + std::string(cmd) +
                        "' names no method. Press Tab after the dot to see the "
                        "methods, or 'inspect " +
                        std::string(cmd.substr(0, cmd.size() - 1U)) + "' to see the object.");
          return;
        }

        std::string message = "'" + std::string(cmd) + "' is not a recognized command.";
        auto names = builtInCommandNames();
        auto hint = formatSuggestionHint(findSuggestions(cmd, names));
        if (!hint.empty())
        {
          message += "\n" + hint;
        }
        message += "\nType 'help' for a list of available commands.";
        reportError("Unknown Command", message);
      }
    }
  }
}

//--------------------------------------------------------------------------------------------------------------
// Update cycle
//--------------------------------------------------------------------------------------------------------------

void CommandEngine::update()
{
  // Give up on calls nobody is going to answer. The kernel has no call timeout of its own, so
  // without this a stalled peer holds a spinner and a 30 Hz redraw for the life of the session.
  if (!pendingCalls_.empty())
  {
    const auto nowForTimeouts = api_.getTime();
    for (auto itr = pendingCalls_.begin(); itr != pendingCalls_.end();)
    {
      if (nowForTimeouts < itr->second.deadline)
      {
        ++itr;
        continue;
      }
      // The body says how long term waited and that nothing was cancelled. Nothing is sent to the peer,
      // so the method may still run and answer later.
      app_.finishPendingCall(
        itr->first,
        renderError("No Answer Yet",
                    itr->second.description + "\nNo answer after " +
                      std::to_string(static_cast<int>(config_.callTimeout.toSeconds())) + " s" +
                      ". The call was not cancelled: the object may still be running it, and the answer "
                      "will be reported if it arrives.\nThe wait is measured on this kernel's clock, so a "
                      "peer whose time is paused or stepped can exceed it while working normally."));
      itr = pendingCalls_.erase(itr);
    }
  }

  // Discovery notices go through the component's own logger rather than straight to the output, so
  // `log level` governs them like any other component's messages.
  auto notifications = store_.drainNotifications();
  for (const auto& n: notifications)
  {
    getLogger()->info(n.message);
  }

  completer_.update(scope_, store_, logRouter_);
}

const Scope& CommandEngine::getScope() const noexcept { return scope_; }
const ObjectStore& CommandEngine::getObjectStore() const noexcept { return store_; }
ObjectStore& CommandEngine::getObjectStore() noexcept { return store_; }

//--------------------------------------------------------------------------------------------------------------
// Command implementations
//--------------------------------------------------------------------------------------------------------------

void CommandEngine::cmdHelp(std::string_view args)
{
  if (!args.empty())
  {
    if (args == "print")
    {
      app_.appendElement(
        ftxui::hbox({ftxui::text("Usage: ") | styles::mutedText(), ftxui::text("<object>.print") | ftxui::bold}));
      app_.appendElement(ftxui::paragraph("Display all property values of an object.") | styles::mutedText());
      return;
    }

    for (const auto& e: getCommandTable())
    {
      if (e.desc.name == args)
      {
        app_.appendElement(ftxui::hbox(
          {ftxui::text("Usage: ") | styles::mutedText(), ftxui::text(std::string(e.desc.usage)) | ftxui::bold}));
        app_.appendElement(ftxui::paragraph(std::string(e.desc.detail)) | styles::mutedText());
        return;
      }
    }
    app_.appendInfo("Unknown command '" + std::string(args) + "'. Type 'help' for a list.");
    return;
  }

  // Category display order.
  static constexpr CommandCategory categoryOrder[] = {CommandCategory::navigation,
                                                      CommandCategory::discovery,
                                                      CommandCategory::queries,
                                                      CommandCategory::logging,
                                                      CommandCategory::inspection,
                                                      CommandCategory::monitoring,
                                                      CommandCategory::general};

  auto padTo = [](std::string_view text, int width)
  {
    std::string s = "  ";
    s += text;
    if (checkedConversion<int>(s.size()) < width)
    {
      s.append(checkedConversion<std::size_t>(width) - s.size(), ' ');
    }
    return s;
  };

  // Find the longest command name for alignment padding.
  std::size_t maxName = 0;
  for (const auto& e: getCommandTable())
  {
    if (e.desc.name == "shutdown")
    {
      continue;
    }
    maxName = std::max(maxName, e.desc.name.size());
  }
  const auto cmdColWidth = checkedConversion<int>(maxName + 4);  // 2 indent + 2 gap

  for (const auto category: categoryOrder)
  {
    bool hasCommands = false;
    for (const auto& e: getCommandTable())
    {
      if (e.desc.category == category && e.desc.name != "shutdown")
      {
        hasCommands = true;
        break;
      }
    }
    if (!hasCommands)
    {
      continue;
    }

    app_.appendElement(ftxui::text(" " + std::string(commandCategoryName(category))) | styles::sectionTitle());
    for (const auto& e: getCommandTable())
    {
      if (e.desc.category != category || e.desc.name == "shutdown")
      {
        continue;
      }
      app_.appendElement(ftxui::hbox({ftxui::text(padTo(e.desc.name, cmdColWidth)) | styles::completionCommand(),
                                      ftxui::text(std::string(e.desc.completionHint)) | styles::descriptionText()}));
    }
    app_.appendElement(ftxui::text(""));
  }

  app_.appendElement(ftxui::text(" Keyboard shortcuts") | styles::sectionTitle());

  struct Shortcut
  {
    std::string_view key;
    std::string_view desc;
  };
  // Everything a key does, because the ones left out of this list were discoverable only by accident:
  // F1, PageUp/PageDown, Ctrl+R and the form's own keys had no mention here or on the page.
  std::vector<Shortcut> shortcuts = {
    {"Tab / Shift+Tab", "Cycle through completions"},
    {"Enter / .", "Accept completion (drill into paths)"},
    {"Escape", "Cancel completion / clear the line / arm exit"},
    {"Ctrl+W", "Erase the word before the cursor"},
    {"Ctrl+R", "Recall the last command containing the line"},
    {"PageUp / PageDown", "Scroll the output ten rows"},
    {"F1", "Run help"},
    {"Ctrl+D", "Shutdown, from an empty line"},
    {"Mouse drag", "Select (auto-copies on release)"},
    {"Ctrl+Y", "Copy selection to clipboard"},
    {"In a form", "Tab moves, Left/Right edits, Ctrl+U clears, Enter submits, Escape cancels"},
  };
  std::size_t maxKey = 0;
  for (const auto& s: shortcuts)
  {
    maxKey = std::max(maxKey, s.key.size());
  }
  const auto scColWidth = checkedConversion<int>(maxKey + 4);

  for (const auto& s: shortcuts)
  {
    app_.appendElement(ftxui::hbox({ftxui::text(padTo(s.key, scColWidth)) | styles::typeName(),
                                    ftxui::text(std::string(s.desc)) | styles::descriptionText()}));
  }
}

void CommandEngine::cmdCd(std::string_view args)
{
  if (args.empty())
  {
    scope_.navigate("/");
    completer_.markScopeDirty();
    app_.setPrompt(scope_.makePrompt());
    return;
  }

  auto target = std::string(args);

  // Special navigation targets bypass validation.
  bool isSpecial = (target == "/" || target == ".." || target == "-" || target.front() == '@');

  // Reject names with spaces (session/bus/group names cannot contain spaces).
  if (!isSpecial && target.find(' ') != std::string::npos)
  {
    reportError("Navigation Error", "Invalid path: names cannot contain spaces.");
    return;
  }

  // The prompt displays the scope with a leading separator, so a path copied out of it arrives with
  // one. Scope::navigate drops it; the checks below have to read the same string navigate will, or
  // they derive an empty session name from it and refuse a path the prompt itself printed.
  std::string_view navTarget = target;
  if (!isSpecial && navTarget.size() > 1U && navTarget.front() == '/')
  {
    navTarget.remove_prefix(1);
  }

  // Any target that names a session has to name one discovery has seen, whether it is a bare session at
  // root or a "session.bus" jump from anywhere. Only the session part can be checked here:
  // getAvailableSources lists "session.bus" for open sessions and the bare name for the rest, so the bus
  // list is unknown until the session is open. Without the check, subscribing to a bus nobody publishes
  // succeeds and leaves the user in a scope where nothing will ever appear.
  const bool namesASession = (scope_.getKind() == Scope::Kind::root) || navTarget.find('.') != std::string_view::npos;
  if (!isSpecial && namesASession)
  {
    const auto sessionName = std::string(navTarget.substr(0, std::min(navTarget.find('.'), navTarget.find('/'))));
    auto available = store_.getAvailableSources();
    const bool known =
      std::any_of(available.begin(),
                  available.end(),
                  [&sessionName](const std::string& src)
                  {
                    return src == sessionName ||
                           (src.size() > sessionName.size() && src.compare(0, sessionName.size(), sessionName) == 0 &&
                            src[sessionName.size()] == '.');
                  });
    if (!known)
    {
      reportError("Navigation Error", "Session '" + sessionName + "' not found. Use 'open' first.");
      return;
    }

    // And the bus, once the session is open: getAvailableSources lists it then as "session.bus", which is
    // exactly when a user types a bus name. Without the check, `cd` accepts a bus nobody publishes and
    // leaves the user in a scope where `ls` is empty for ever, with no error.
    const auto busStart = navTarget.find('.');
    if (busStart != std::string_view::npos)
    {
      auto busEnd = navTarget.find('/', busStart + 1);
      const auto busName = std::string(navTarget.substr(busStart + 1, busEnd - busStart - 1));
      const std::string prefix = sessionName + ".";

      std::vector<std::string> knownBuses;
      for (const auto& src: available)
      {
        if (src.size() > prefix.size() && src.compare(0, prefix.size(), prefix) == 0)
        {
          knownBuses.push_back(src.substr(prefix.size()));
        }
      }

      if (!busName.empty() && !knownBuses.empty() &&
          std::find(knownBuses.begin(), knownBuses.end(), busName) == knownBuses.end())
      {
        std::ostringstream message;
        message << "Session '" << sessionName << "' publishes no bus '" << busName << "'. It has: ";
        for (std::size_t i = 0; i < knownBuses.size(); ++i)
        {
          message << (i == 0 ? "" : ", ") << knownBuses[i];
        }
        reportError("Navigation Error", message.str());
        return;
      }
    }
  }

  // A query is the one namespace term owns outright: it creates them, holds the list and reserves the
  // separator in their names, so it can say with certainty that one does not exist. Sessions and buses
  // are only partly known, because discovery cannot list a closed session's buses. A group is not a thing
  // in Sen at all, only an artefact of splitting the object names present this instant, so "no such
  // group" is a statement term cannot make.
  if (!isSpecial && target.front() == '@' && target.size() > 1U)
  {
    const auto wanted = target.substr(1);
    auto queries = store_.getQueries();
    const bool known =
      std::any_of(queries.begin(), queries.end(), [&wanted](const auto& q) { return q.name == wanted; });
    if (!known)
    {
      std::ostringstream message;
      message << "No query named '" << wanted << "'. Use 'queries' to list them, or 'query " << wanted
              << " <selection>' to create it.";
      reportError("Navigation Error", message.str());
      return;
    }
  }

  if (!scope_.navigate(args))
  {
    reportError("Navigation Error", "Cannot navigate to '" + target + "' from " + std::string(scope_.getPath()));
    return;
  }

  // Open whatever bus the scope now names, rather than the string that was typed: opening the typed
  // string missed `cd main` from a session scope, so the same bus was populated when reached as
  // `cd local.main` and empty when reached in two steps.
  if (auto busAddr = scope_.getBusAddress(); !busAddr.empty() && !store_.isSourceOpen(busAddr))
  {
    if (auto err = store_.openSource(busAddr); err.isError())
    {
      reportError("Cannot Open Source", "Failed to open '" + busAddr + "': " + err.getError());
    }
  }

  completer_.markScopeDirty();
  app_.setPrompt(scope_.makePrompt());

  // Say when a group scope holds nothing, phrased as the present rather than a verdict on the path: a
  // group that is empty now may be filled by a component still starting up, so refusing would turn an
  // early `cd` into an error indistinguishable from a typo. Saying nothing is worse, because an empty
  // `ls` cannot be told from a group that never matched. Only when the bus has objects and this group has
  // none, so a bus nobody has populated yet stays quiet.
  if (scope_.getKind() == Scope::Kind::group)
  {
    // Ask a bus-level scope rather than matching the address against the name here: one implementation
    // of "is this object under that scope", not a fourth place that takes names apart.
    Scope busScope;
    busScope.navigate(scope_.getBusAddress());

    const auto& objects = store_.getObjects();
    const bool busHasObjects = std::any_of(
      objects.begin(), objects.end(), [&busScope](const auto& obj) { return busScope.contains(obj->getLocalName()); });
    const bool hereHasObjects = std::any_of(
      objects.begin(), objects.end(), [this](const auto& obj) { return scope_.contains(obj->getLocalName()); });
    if (busHasObjects && !hereHasObjects)
    {
      app_.appendInfo("(no objects here yet)");
    }
  }
}

void CommandEngine::cmdPwd(std::string_view /*args*/) { app_.appendOutput(scope_.getPath()); }

namespace
{

/// The "[QualifiedName]" annotation a tree node carries for an object.
std::string annotationFor(const Object& obj)
{
  std::string annotation = "[";
  annotation += obj.getClass()->getQualifiedName();
  annotation += "]";
  return annotation;
}

}  // namespace

void CommandEngine::cmdLs(std::string_view args)
{
  if (store_.getObjects().empty())
  {
    app_.appendOutput("(no objects)");
    return;
  }

  // ls @queryname, and `ls` while in query scope, render the same tree from the same query, so both
  // arrive here and differ only in where the name comes from.
  std::string_view queryName;
  if (!args.empty() && args.front() == '@')
  {
    queryName = args.substr(1);
  }
  else if (scope_.getKind() == Scope::Kind::query && args.empty())
  {
    queryName = scope_.getQueryName();
  }

  if (!queryName.empty())
  {
    auto matchedObjects = store_.getQueryObjects(queryName);
    if (matchedObjects.empty())
    {
      app_.appendOutput("(no objects matching query '" + std::string(queryName) + "')");
      return;
    }

    TreeNode tempTree;
    for (const auto& obj: matchedObjects)
    {
      auto relName = scope_.relativeName(obj->getLocalName());
      if (relName.empty())
      {
        // Empty when the object's own name is the group the scope stands in. Splitting that gives
        // one empty component, which renders as a blank row carrying a class annotation.
        continue;
      }
      auto pathComponents = impl::split(relName, '.');
      auto* node = tempTree.getOrCreateChild(pathComponents);
      node->setAnnotation(annotationFor(*obj));
    }
    tempTree.render([this](ftxui::Element line) { app_.appendElement(std::move(line)); });
    return;
  }

  rebuildTreeIfNeeded();

  // If a path argument was given, render only the subtree rooted at that path.
  if (!args.empty())
  {
    // The prompt and `cd` write a nested group with '/', so accept that here too rather than making
    // one path work in one command and not the other.
    auto path = std::string(args);
    std::replace(path.begin(), path.end(), '/', '.');
    auto pathComponents = impl::split(path, '.');
    auto* subtree = cachedTree_.findChild(pathComponents);
    if (subtree == nullptr)
    {
      app_.appendOutput("(no objects matching '" + std::string(args) + "')");
      return;
    }
    subtree->render([this](ftxui::Element line) { app_.appendElement(std::move(line)); });
    return;
  }

  cachedTree_.render([this](ftxui::Element line) { app_.appendElement(std::move(line)); });
}

void CommandEngine::rebuildTreeIfNeeded()
{
  auto gen = store_.getGeneration();
  if (gen == cachedTreeGeneration_ && scope_.getKind() == cachedTreeScopeKind_ &&
      scope_.getPath() == cachedTreeScopePath_)
  {
    return;
  }

  cachedTree_.clear();
  cachedTreeGeneration_ = gen;
  cachedTreeScopeKind_ = scope_.getKind();
  cachedTreeScopePath_ = scope_.getPath();

  int scopeDepth = 0;
  switch (scope_.getKind())
  {
    case Scope::Kind::root:
      scopeDepth = 0;
      break;
    case Scope::Kind::session:
      scopeDepth = 1;
      break;
    case Scope::Kind::bus:
      scopeDepth = 2;
      break;
    case Scope::Kind::group:
      scopeDepth = 3;
      break;
    case Scope::Kind::query:
      scopeDepth = 0;
      break;
  }

  for (const auto& obj: store_.getObjects())
  {
    auto localName = std::string(obj->getLocalName());

    if (!scope_.contains(localName))
    {
      continue;
    }

    auto relName = scope_.relativeName(localName);
    if (relName.empty())
    {
      continue;
    }

    auto pathComponents = impl::split(relName, '.');
    auto* node = cachedTree_.getOrCreateChild(pathComponents);

    // Build annotation: [QualifiedTypeName, Remote/Native]
    std::string annotation = "[";
    annotation += obj->getClass()->getQualifiedName();
    if (obj->asNativeObject() != nullptr)
    {
      annotation += ", Native";
    }
    else if (obj->asProxyObject() != nullptr && obj->asProxyObject()->isRemote())
    {
      annotation += ", Remote";
    }
    annotation += "]";
    node->setAnnotation(std::move(annotation));
    node->setKind(TreeNode::Kind::object);

    // Walk up and label parent nodes
    int depth = 0;
    {
      auto* p = node->getParent();
      while (p != nullptr && p != &cachedTree_)
      {
        ++depth;
        p = p->getParent();
      }
    }

    auto* parent = node->getParent();
    int level = depth;
    while (parent != nullptr && parent != &cachedTree_)
    {
      if (parent->getAnnotation().empty())
      {
        int absoluteLevel = scopeDepth + level;
        if (absoluteLevel == 1)
        {
          parent->setAnnotation("[session]");
          parent->setKind(TreeNode::Kind::session);
        }
        else if (absoluteLevel == 2)
        {
          parent->setAnnotation("[bus]");
          parent->setKind(TreeNode::Kind::bus);
        }
        else
        {
          parent->setAnnotation("[group]");
          parent->setKind(TreeNode::Kind::group);
        }
      }
      --level;
      parent = parent->getParent();
    }
  }
}

void CommandEngine::cmdOpen(std::string_view args)
{
  if (args.empty())
  {
    reportError("Missing Argument", "Usage: open <session.bus>");
    return;
  }

  if (auto err = store_.openSource(args); err.isError())
  {
    reportError("Open Failed", err.getError());
  }
}

void CommandEngine::cmdClose(std::string_view args)
{
  if (args.empty())
  {
    reportError("Missing Argument", "Usage: close <source>");
    return;
  }

  if (auto err = store_.closeSource(args); err.isError())
  {
    reportError("Close Failed", err.getError());
  }
}

void CommandEngine::cmdQuery(std::string_view args)
{
  if (args.empty())
  {
    reportError("Missing Arguments", "Usage: query <name> <SELECT...>\n       query rm <name>");
    return;
  }

  auto [first, rest] = splitCommand(args);

  // query rm <name>
  if (first == "rm")
  {
    if (rest.empty())
    {
      reportError("Missing Argument", "Usage: query rm <name>");
      return;
    }
    if (auto err = store_.removeQuery(rest); err.isError())
    {
      reportError("Remove Failed", err.getError());
    }
    return;
  }

  // query <name> <SELECT...>
  if (rest.empty())
  {
    reportError("Missing Query", "Usage: query <name> <SELECT...>");
    return;
  }

  if (auto err = store_.createQuery(first, rest); err.isError())
  {
    reportError("Query Failed", err.getError());
  }
}

void CommandEngine::cmdQueries(std::string_view /*args*/)
{
  auto queries = store_.getQueries();
  if (queries.empty())
  {
    app_.appendInfo("No named queries.  Use 'query <name> <SELECT...>' to create one.");
    return;
  }

  // Highlight query-language keywords in a selection string.
  static const std::vector<std::string> queryKeywords = {
    "SELECT",
    "FROM",
    "WHERE",
    "AND",
    "OR",
    "NOT",
    "IN",
    "BETWEEN",
  };
  auto highlightSelection = [&](const std::string& selection) -> ftxui::Element
  {
    ftxui::Elements parts;
    std::istringstream stream(selection);
    std::string token;
    bool first = true;
    while (stream >> token)
    {
      if (!first)
      {
        parts.push_back(ftxui::text(" "));
      }
      first = false;

      bool isKeyword = std::find(queryKeywords.begin(), queryKeywords.end(), token) != queryKeywords.end();
      bool isWildcard = (token == "*");

      if (isKeyword)
      {
        parts.push_back(ftxui::text(token) | ftxui::bold | styles::typeName());
      }
      else if (isWildcard)
      {
        parts.push_back(ftxui::text(token) | ftxui::bold);
      }
      else
      {
        parts.push_back(ftxui::text(token));
      }
    }
    return ftxui::hbox(std::move(parts));
  };

  std::vector<text_table::Row> tableData;
  tableData.push_back({
    {"Name", ftxui::text("Name") | ftxui::bold | styles::mutedText()},
    {"Selection", ftxui::text("Selection") | ftxui::bold | styles::mutedText()},
  });
  for (const auto& q: queries)
  {
    tableData.push_back({
      text_table::Cell(q.name),
      text_table::Cell(q.selection, highlightSelection(q.selection)),
    });
  }

  app_.appendElement(ftxui::hbox({ftxui::text("  "), text_table::render(std::move(tableData))}));
}

void CommandEngine::cmdLog(std::string_view args)
{
  if (args.empty())
  {
    // Show current level and list loggers
    auto levelName = spdlog::level::to_string_view(LogRouter::getGlobalLevel());
    app_.appendOutput("Global log level: " + std::string(levelName.data(), levelName.size()));
    app_.appendOutput("");

    auto loggers = logRouter_.listLoggers();
    if (loggers.empty())
    {
      app_.appendOutput("(no loggers registered)");
    }
    else
    {
      app_.appendOutput("Registered loggers:");
      for (const auto& info: loggers)
      {
        auto lvl = spdlog::level::to_string_view(info.level);
        auto name = info.name.empty() ? "(default)" : info.name;
        app_.appendOutput("  " + name + "  [" + std::string(lvl.data(), lvl.size()) + "]");
      }
    }
    return;
  }

  auto [sub, rest] = splitCommand(args);

  if (sub == "level")
  {
    if (rest.empty())
    {
      reportError("Missing Argument",
                  "Usage: log level <level>\n       log level <logger> <level>\n"
                  "Levels: trace, debug, info, warn, error, critical, off");
      return;
    }

    auto [first, second] = splitCommand(rest);

    if (second.empty())
    {
      // Global level: log level <level>
      spdlog::level::level_enum level {};
      if (!LogRouter::parseLevel(first, level))
      {
        reportError("Invalid Level",
                    "'" + std::string(first) +
                      "' is not a valid log level.\nValid: trace, debug, info, warn, error, critical, off");
        return;
      }
      auto lvl = spdlog::level::to_string_view(level);
      if (!logRouter_.setGlobalLevel(level))
      {
        reportError("Level Refused", "the kernel refused the level " + std::string(lvl.data(), lvl.size()));
        return;
      }
      app_.appendInfo("Global log level set to " + std::string(lvl.data(), lvl.size()));
    }
    else
    {
      // Per-logger level: log level <logger> <level>
      spdlog::level::level_enum level {};
      if (!LogRouter::parseLevel(second, level))
      {
        reportError("Invalid Level",
                    "'" + std::string(second) +
                      "' is not a valid log level.\nValid: trace, debug, info, warn, error, critical, off");
        return;
      }
      if (!logRouter_.setLoggerLevel(first, level))
      {
        reportError("Logger Not Found", "No logger named '" + std::string(first) + "'");
        return;
      }
      auto lvl = spdlog::level::to_string_view(level);
      app_.appendInfo("Logger '" + std::string(first) + "' level set to " + std::string(lvl.data(), lvl.size()));
    }
    return;
  }

  std::ostringstream usage;
  usage << "Usage: log              " << unicode::middleDot << " show loggers and levels\n"
        << "       log level <lvl>  " << unicode::middleDot << " set global level\n"
        << "       log level <name> <lvl> " << unicode::middleDot << " set logger level";
  reportError("Unknown Subcommand", usage.str());
}

void CommandEngine::cmdClear(std::string_view /*args*/) { app_.clearCommandPane(); }

void CommandEngine::cmdTheme(std::string_view args)
{
  const auto& enumType = *MetaTypeTrait<ThemeStyle>::meta();

  if (args.empty())
  {
    std::string available;
    for (const auto& e: enumType.getEnums())
    {
      if (!available.empty())
      {
        available += ", ";
      }
      available += e.name;
    }
    reportError("Usage", "theme <name>\nAvailable: " + available);
    return;
  }

  const auto* enumerator = enumType.getEnumFromName(args);
  if (enumerator == nullptr)
  {
    reportError("Unknown Theme", "'" + std::string(args) + "' is not a known theme.");
    return;
  }

  setActiveTheme(themeForStyle(static_cast<ThemeStyle>(enumerator->key)));
  app_.appendInfo("Theme changed to '" + std::string(args) + "'.");
}

void CommandEngine::cmdShutdown(std::string_view /*args*/)
{
  app_.appendInfo("Shutting down...");
  // Match Ctrl+D: flag the app, and let the execLoop callback in component.cpp call requestKernelStop on
  // the next iteration. Calling it here races the main thread's runner teardown against the rest of this
  // event dispatch and can deadlock.
  app_.requestShutdown();
}

Span<const CommandDescriptor> CommandEngine::getCommandDescriptors()
{
  static std::vector<CommandDescriptor> descs;
  if (descs.empty())
  {
    auto table = getCommandTable();
    descs.reserve(table.size());
    for (const auto& e: getCommandTable())
    {
      descs.push_back(e.desc);
    }
  }
  return descs;
}

void CommandEngine::echoCommand(std::string_view input, bool isError)
{
  auto color = isError ? styles::echoError() : styles::echoSuccess();
  auto now = api_.getTime();
  auto timeStr = formatTime(now, config_.timeStyle);
  std::string shortTime;
  if (timeStr.size() >= 19U)
  {
    shortTime = timeStr.substr(11U, 8U);
  }
  app_.appendElement(ftxui::hbox({ftxui::text(std::string(unicode::promptSymbol) + " ") | ftxui::bold | color,
                                  ftxui::paragraph(std::string(input)),
                                  ftxui::filler(),
                                  ftxui::text(shortTime) | styles::mutedText()}));
}

void CommandEngine::reportError(std::string_view title, std::string_view message)
{
  app_.appendElement(renderError(title, message));
}

void CommandEngine::handlePrintCommand(std::string_view input,
                                       const std::shared_ptr<Object>& target,
                                       const ClassType& classType)
{
  echoCommand(input);
  auto properties = classType.getProperties(ClassType::SearchMode::includeParents);
  if (properties.empty())
  {
    app_.appendInfo("(no properties)");
  }
  else
  {
    for (const auto& prop: properties)
    {
      auto value = target->getPropertyUntyped(prop.get());
      app_.appendElement(renderPropertyValue(*prop, value));
    }
  }
}

void CommandEngine::reportUnknownMethod(std::string_view input,
                                        std::string_view objectName,
                                        std::string_view methodName,
                                        const ClassType& classType)
{
  echoCommand(input, /*isError=*/true);

  // Gather method and property-accessor names for "did you mean?" suggestions.
  std::vector<std::string> names;
  for (const auto& m: classType.getMethods(ClassType::SearchMode::includeParents))
  {
    names.emplace_back(m->getName());
  }
  for (const auto& prop: classType.getProperties(ClassType::SearchMode::includeParents))
  {
    names.emplace_back(prop->getGetterMethod().getName());
    if (prop->getCategory() == PropertyCategory::dynamicRW)
    {
      names.emplace_back(prop->getSetterMethod().getName());
    }
  }

  std::string message = "Object '" + std::string(objectName) + "' (" + std::string(classType.getName()) +
                        ") has no method '" + std::string(methodName) + "'.";
  auto hint = formatSuggestionHint(findSuggestions(methodName, names));
  if (!hint.empty())
  {
    message += "\n" + hint;
  }
  reportError("Unknown Method", message);
}

void CommandEngine::invokeMethodAsync(std::string_view input,
                                      std::string_view methodName,
                                      const std::shared_ptr<Object>& target,
                                      const Method* method,
                                      VarList argValues)
{
  auto callId = app_.startPendingCall(std::string(input));

  auto now = api_.getTime();
  pendingCalls_.try_emplace(callId, PendingCall {std::string(input), now + config_.callTimeout});

  auto timeStr = formatTime(now, config_.timeStyle);
  std::string shortTime;
  if (timeStr.size() >= 19U)
  {
    shortTime = timeStr.substr(11U, 8U);
  }

  target->invokeUntyped(method,
                        argValues,
                        MethodCallback<Var> {api_.getWorkQueue(),
                                             [this,
                                              callId,
                                              inputStr = std::string(input),
                                              name = std::string(methodName),
                                              retType = method->getReturnType(),
                                              timestamp = std::move(shortTime)](const MethodResult<Var>& result)
                                             {
                                               // Nothing upstream catches an exception from here:
                                               // runner.cpp's EXCEPTION_WRAP_BLOCK expands to a bare
                                               // do/while, so one escaping this lambda ends the process
                                               // and every component in it. The catch arms below are
                                               // what stops that.
                                               //
                                               // This captures `this` and is held by the target object,
                                               // which can outlive the engine. What keeps it safe is
                                               // `Runner::signalThreadToStop` disabling the work queue
                                               // before it raises the stop flag, so a queued callback
                                               // never runs once teardown has begun.
                                               //
                                               // `hasPendingCall` is checked before `*retType` is read:
                                               // a TypeHandle can be non-owning, and a late answer is
                                               // the case where the class metadata may be gone.
                                               if (!hasPendingCall(callId))
                                               {
                                                 app_.appendInfo("A late answer arrived for '" + inputStr +
                                                                 "', after term stopped waiting for it. It is "
                                                                 "not shown: the type it should be drawn "
                                                                 "against may be gone.");
                                                 return;
                                               }

                                               try
                                               {
                                                 finishMethodCall(callId, inputStr, name, *retType, timestamp, result);
                                               }
                                               // Both arms say the same thing because both mean the same
                                               // thing: the call returned and term could not draw the
                                               // result. Reporting it as a call error would tell the user
                                               // their method failed when it had worked.
                                               catch (const std::exception& e)
                                               {
                                                 failPendingCall(callId,
                                                                 name +
                                                                   " returned, but term could not draw the "
                                                                   "result: " +
                                                                   e.what(),
                                                                 "Result Not Drawn");
                                               }
                                               catch (...)
                                               {
                                                 failPendingCall(callId,
                                                                 name +
                                                                   " returned, but term could not draw the "
                                                                   "result.",
                                                                 "Result Not Drawn");
                                               }
                                             }});
}

void CommandEngine::finishMethodCall(std::size_t callId,
                                     const std::string& inputStr,
                                     const std::string& methodName,
                                     const Type& returnType,
                                     const std::string& timestamp,
                                     const MethodResult<Var>& result)
{
  const bool ok = result.isOk();
  std::string marker = ok ? std::string(unicode::check) + " " : std::string(unicode::cross) + " ";
  auto color = ok ? styles::echoSuccess() : styles::echoError();
  auto echoElement = ftxui::hbox({ftxui::text(marker) | ftxui::bold | color,
                                  ftxui::paragraph(inputStr),
                                  ftxui::filler(),
                                  ftxui::text(timestamp) | styles::mutedText()});

  // Echo + result in one element so they stay together.
  ftxui::Elements combined;
  combined.push_back(echoElement);

  if (ok)
  {
    if (!returnType.isVoidType())
    {
      combined.push_back(renderMethodResult(result.getValue(), returnType));
    }
  }
  else
  {
    std::string errMsg;
    try
    {
      std::rethrow_exception(result.getError());
    }
    catch (const std::exception& e)
    {
      errMsg = methodName + ": " + e.what();
    }
    catch (...)
    {
      errMsg = methodName + ": unknown error";
    }
    combined.push_back(renderError("Call Error", errMsg));
  }

  // Checked again: the caller tested it before evaluating the return type, and between that test and
  // here nothing can have changed, because both run on the component thread. Reported by the caller
  // when it is gone.
  if (pendingCalls_.erase(callId) == 0U)
  {
    return;
  }
  app_.finishPendingCall(callId, ftxui::vbox(std::move(combined)));
}

bool CommandEngine::hasPendingCall(std::size_t callId) const { return pendingCalls_.count(callId) != 0U; }

void CommandEngine::failPendingCall(std::size_t callId, const std::string& message, const std::string& title)
{
  // The slot may already have been consumed before the throw, and finishing one twice would append
  // the answer twice.
  if (pendingCalls_.erase(callId) != 0U)
  {
    app_.finishPendingCall(callId, renderError(title, message));
  }
}

namespace
{

/// The call the user should have typed, for an error message.
[[nodiscard]] std::string buildSignatureHint(std::string_view objectName,
                                             std::string_view methodName,
                                             Span<const Arg> methodArgs)
{
  std::ostringstream hint;
  hint << objectName << "." << methodName;
  if (methodArgs.empty())
  {
    hint << " (no arguments)";
    return hint.str();
  }
  hint << " ";
  for (std::size_t i = 0; i < methodArgs.size(); ++i)
  {
    if (i > 0)
    {
      hint << ", ";
    }
    hint << "<" << methodArgs[i].type->getName() << ">";
  }
  return hint.str();
}

}  // namespace

void CommandEngine::reportUnknownObject(std::string_view input, std::string_view objectName)
{
  // A dot makes this look like an object.method call, so it is reported as a failed call rather than
  // falling through to "Unknown Command": that way the message can suggest objects that do exist.
  echoCommand(input, /*isError=*/true);
  std::ostringstream message;
  message << "No object named '" << objectName << "' in the current scope.";
  auto hint = formatSuggestionHint(completer_.findObjectSuggestions(objectName));
  if (!hint.empty())
  {
    message << "\n" << hint;
  }
  message << "\nUse 'ls' to see visible objects.";
  reportError("Unknown Object", message.str());
}

const Method* CommandEngine::findMethodOrAccessor(const ClassType& classType,
                                                  std::string_view methodName,
                                                  const Property*& setterProperty)
{
  if (const Method* method = classType.searchMethodByName(methodName); method != nullptr)
  {
    return method;
  }
  // A property's getter and setter are not in getMethods(), so they are reached through the property.
  for (const auto& prop: classType.getProperties(ClassType::SearchMode::includeParents))
  {
    if (prop->getGetterMethod().getName() == methodName)
    {
      return &prop->getGetterMethod();
    }
    if (prop->getCategory() == PropertyCategory::dynamicRW && prop->getSetterMethod().getName() == methodName)
    {
      setterProperty = prop.get();
      return &prop->getSetterMethod();
    }
  }
  return nullptr;
}

bool CommandEngine::parseAndAdaptArgs(std::string_view input,
                                      std::string_view args,
                                      const Method& method,
                                      std::string_view signatureHint,
                                      VarList& argValues)
{
  auto methodArgs = method.getArgs();
  try
  {
    argValues = parseArgs(&method, args);
  }
  catch (const std::exception&)
  {
    echoCommand(input, /*isError=*/true);
    std::ostringstream message;
    message << "Could not parse '" << args << "'. Expected format:\n  " << signatureHint
            << "\nSeparate arguments with spaces or commas; wrap string values in double quotes.";
    reportError("Invalid arguments", message.str());
    return false;
  }

  if (argValues.size() > methodArgs.size())
  {
    echoCommand(input, /*isError=*/true);
    std::ostringstream message;
    message << "Too many arguments: expected " << methodArgs.size() << ", got " << argValues.size()
            << ".\nExpected format:\n  " << signatureHint;
    reportError("Invalid arguments", message.str());
    return false;
  }

  // Adapting here rather than at the call catches a type error while the typed text is still on screen.
  for (std::size_t i = 0; i < argValues.size(); ++i)
  {
    if (auto result = impl::adaptVariant(*methodArgs[i].type, argValues[i]); result.isError())
    {
      echoCommand(input, /*isError=*/true);
      std::ostringstream message;
      message << "Argument '" << methodArgs[i].name << "': " << result.getError() << "\nExpected format:\n  "
              << signatureHint;
      reportError("Invalid arguments", message.str());
      return false;
    }
  }
  return true;
}

bool CommandEngine::tryResolveObjectMethod(std::string_view input, std::string_view cmd, std::string_view args)
{
  // `reject`: "obj." names no method to call. Completion asks the same function the other question, and
  // accepting a candidate is what leaves the trailing dot on the line.
  std::string_view objectName;
  std::string_view methodName;
  if (!splitObjectMember(cmd, TrailingSeparator::reject, objectName, methodName))
  {
    return false;
  }

  auto target = completer_.findObject(objectName);
  if (!target)
  {
    reportUnknownObject(input, objectName);
    return true;
  }

  auto classType = target->getClass();
  const Property* setterProperty = nullptr;  // non-null when the match is a property setter
  const Method* method = findMethodOrAccessor(*classType, methodName, setterProperty);

  // Virtual "print" method: display all properties of the object
  if (method == nullptr && methodName == "print")
  {
    handlePrintCommand(input, target, *classType);
    return true;
  }

  if (method == nullptr)
  {
    reportUnknownMethod(input, objectName, methodName, *classType);
    return true;
  }

  auto methodArgs = method->getArgs();

  // PropertyGetter shortcut: read cached property value without invoking. No form needed
  // because getters take no arguments.
  auto* isGetter = std::get_if<PropertyGetter>(&method->getPropertyRelation());
  if (isGetter != nullptr)
  {
    echoCommand(input);
    auto value = target->getPropertyUntyped(isGetter->property);
    app_.appendElement(renderPropertyValue(*isGetter->property, value));
    return true;
  }

  const std::string signatureHint = buildSignatureHint(objectName, methodName, methodArgs);

  VarList argValues;
  if (!parseAndAdaptArgs(input, args, *method, signatureHint, argValues))
  {
    return true;
  }

  // Missing arguments: open a guided-input form. Property setters seed with the current value.
  if (argValues.size() < methodArgs.size())
  {
    if (setterProperty != nullptr && argValues.empty())
    {
      Var currentValue = target->getPropertyUntyped(setterProperty);
      if (auto r = impl::adaptVariant(*methodArgs[0].type, currentValue); r.isOk())
      {
        argValues.push_back(std::move(currentValue));
      }
    }
    if (auto form = ArgForm::build(*method, objectName, argValues); form.has_value())
    {
      app_.openArgForm(std::move(*form));
      return true;
    }
    // Form-incompatible method: show signature or error.
    if (args.empty())
    {
      echoCommand(input);
      app_.appendElement(renderMethodSignature(objectName, *method));
      return true;
    }
    echoCommand(input, /*isError=*/true);
    reportError("Invalid arguments",
                "Not enough arguments: expected " + std::to_string(methodArgs.size()) + ", got " +
                  std::to_string(argValues.size()) + ".\nExpected format:\n  " + signatureHint);
    return true;
  }

  // Async invocation with pending spinner.
  invokeMethodAsync(input, methodName, target, method, std::move(argValues));
  return true;
}

}  // namespace sen::components::term
