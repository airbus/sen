// === command_engine.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "command_engine.h"

// local
#include "arg_form.h"
#include "byte_format.h"
#include "parse_utils.h"
#include "signature_renderer.h"
#include "styles.h"
#include "suggester.h"
#include "text_table.h"
#include "tree_view.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"

// asio
#include <asio/ip/host_name.hpp>

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

// spdlog
#include <spdlog/spdlog.h>

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

namespace
{

/// Hidden replies to specific inputs. Two modes:
///   fullInput != nullptr : match the entire trimmed input verbatim.
///   cmdToken != nullptr  : match the command token alone (first whitespace-delimited word).
/// Full-input entries are tried before the command dispatcher so phrases that share their
/// first word with a real command ("open the pod bay doors") still fire. Command-token
/// entries are tried after the dispatcher and after object-method resolution, so a real
/// command or method can never be shadowed.
struct Egg
{
  const char* cmdToken = nullptr;
  const char* fullInput = nullptr;
  const char* response = nullptr;
};

constexpr Egg eggs[] = {
  // Full-input phrases. Kept to a minimum: only short, iconic triggers a user might
  // plausibly type on a dare. They run before the dispatcher so their first word can
  // be a real command (none of these currently are, but the priority is preserved
  // for future entries like `open the pod bay doors`).
  {nullptr, "42", "42. But what was the question?"},
  {nullptr, "xyzzy", "Nothing happens."},

  // Command-token entries. These catch common muscle-memory slips from other tools
  // and redirect the user to the real equivalent. Tried after the dispatcher and
  // after object-method resolution, so a legitimate command or method is never
  // shadowed.

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
  {"rm", nullptr, "No destruction here. Try 'query rm' or 'unwatch'."},

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
                   "Clear the command output pane.",
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
                   "Stream event emissions to the log pane.\n"
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
                   "List all registered types, optionally filtered by prefix.\n"
                   "  types                 All types\n"
                   "  types term_showcase   Only types in that package",
                   "list registered types"},
     &CommandEngine::cmdTypes},
    {{"units",     CommandCategory::inspection,
                   "units [filter]",
                   "List all registered units, optionally filtered by category.\n"
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
    {{"unwatch",   CommandCategory::monitoring,
                   "unwatch <object>[.<property>] | all",
                   "Stop watching properties.\n"
                   "  unwatch obj.prop      Stop one watch\n"
                   "  unwatch obj           Stop all watches on an object\n"
                   "  unwatch all           Clear every watch",
                   "stop watching an object or a single property", /*tuiOnly=*/true},
     &CommandEngine::cmdUnwatch},
    {{"watch",     CommandCategory::monitoring,
                   "watch <object>[.<property>]",
                   "Monitor property values in the watch pane (F5).\n"
                   "  watch obj             Watch all properties\n"
                   "  watch obj.prop        Watch one property\n"
                   "Watches survive object removal and reconnect automatically.",
                   "stream value changes of an object or a single property", /*tuiOnly=*/true},
     &CommandEngine::cmdWatch},
    {{"watches",   CommandCategory::monitoring,
                   "watches",
                   "List active property watches.",
                   "list active watches", /*tuiOnly=*/true},
     &CommandEngine::cmdWatches},
  };
  // clang-format on
  return {table, std::size(table)};
}

namespace
{

/// Return built-in command names for "did you mean?" suggestions.
std::vector<std::string> builtInCommandNames(App::Mode mode)
{
  auto table = CommandEngine::getCommandTable();
  std::vector<std::string> names;
  names.reserve(table.size());
  for (const auto& e: table)
  {
    if (e.desc.tuiOnly && mode == App::Mode::repl)
    {
      continue;
    }
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

  // Single string argument: pass as-is without JSON quoting.
  if (methodArgs.size() == 1 && methodArgs[0].type->isStringType() && args[0] != '"')
  {
    return {Var(std::string(args))};
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
  : config_(config)
  , api_(api)
  , app_(app)
  , logRouter_(logRouter)
  , completer_(completer)
  , store_(api)
  , hostName_(asio::ip::host_name())
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

  if (!config_.initialScope.empty())
  {
    scope_.navigate(config_.initialScope);
  }

  app_.setStatusIdentity(api_.getAppName(), hostName_);
  app_.setPrompt(scope_.makePrompt());
  app_.setStatus(0U,
                 store_.getObjectCount(),
                 scope_.getKind() == Scope::Kind::root,
                 0U,
                 store_.getOpenSources().size(),
                 listeners_.size(),
                 scope_.getPath(),
                 "",
                 "",
                 "",
                 0.0,
                 0.0);
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

  // Full-input easter eggs run before the dispatcher so puns like `open the pod bay
  // doors` win against the real `open` command. Handled here so they still get the
  // normal post-processing (blank-line separator, status refresh).
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

    if (entry != nullptr && entry->desc.tuiOnly && app_.displayMode() == App::Mode::repl)
    {
      echoCommand(input, /*isError=*/true);
      reportError("Not available", "'" + std::string(cmd) + "' requires TUI mode (displayMode: tui).");
    }
    else if (entry != nullptr)
    {
      echoCommand(input);
      (this->*entry->handler)(args);
    }
    else if (!tryResolveObjectMethod(input, cmd, args))
    {
      // Command-token easter eggs run after the dispatcher + object-method resolution so
      // a real command or method can never be shadowed.
      if (const auto* egg = tryEggCmdToken(cmd); egg != nullptr)
      {
        echoCommand(input);
        app_.appendInfo(egg);
      }
      else
      {
        echoCommand(input, /*isError=*/true);
        std::string message = "'" + std::string(cmd) + "' is not a recognized command.";
        auto names = builtInCommandNames(app_.displayMode());
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

  // Form's re-entrant execute() adds its own separator.
  if (!formBefore && app_.hasActiveForm())
  {
    return;
  }

  app_.appendOutput("");
  statusDirty_ = true;
  completer_.markListsDirty();
}

//--------------------------------------------------------------------------------------------------------------
// Update cycle
//--------------------------------------------------------------------------------------------------------------

void CommandEngine::update()
{
  const auto now = api_.getTime();
  const auto seconds = now.sinceEpoch().getNanoseconds() / 1'000'000'000;
  const bool secondChanged = (seconds != lastTimeSeconds_);

  if (secondChanged)
  {
    lastTimeSeconds_ = seconds;
    const std::string timeStr = (config_.timeStyle == TimeStyle::local) ? now.toLocalString() : now.toUtcString();
    if (timeStr.size() >= 19U)
    {
      lastTimeStr_ = timeStr.substr(11U, 8U);
      lastTimeStr_ += (config_.timeStyle == TimeStyle::local) ? " LT" : " UTC";
    }
    else
    {
      lastTimeStr_.clear();
    }
  }

  auto notifications = store_.drainNotifications();
  if (!notifications.empty())
  {
    auto fullTime = now.toLocalString();
    auto timeStr = (fullTime.size() >= 19U) ? fullTime.substr(11U, 8U) : fullTime;
    for (const auto& n: notifications)
    {
      app_.appendLogElement(ftxui::hbox({ftxui::text(timeStr + " ") | styles::mutedText(),
                                         ftxui::text("[term] ") | styles::echoSuccess() | ftxui::bold,
                                         ftxui::text(n.message) | styles::mutedText()}));
    }
  }

  bool statusDirty = secondChanged || (store_.getGeneration() != lastStatusGeneration_) || statusDirty_;
  if (statusDirty)
  {
    lastStatusGeneration_ = store_.getGeneration();
    statusDirty_ = false;

    // Compute transport throughput rates.
    auto nowSteady = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(nowSteady - prevTransportTime_).count();
    if (elapsed >= 1.0)
    {
      auto info = api_.fetchMonitoringInfo();
      const auto& ts = info.transportStats;
      auto txBytes = (ts.udpSentBytes + ts.tcpSentBytes);
      auto rxBytes = (ts.udpReceivedBytes + ts.tcpReceivedBytes);
      auto prevTx = (prevTransportStats_.udpSentBytes + prevTransportStats_.tcpSentBytes);
      auto prevRx = (prevTransportStats_.udpReceivedBytes + prevTransportStats_.tcpReceivedBytes);

      bool hasTransport = (txBytes + rxBytes) > 0;
      if (hasTransport)
      {
        auto txRate = checkedConversion<double>(txBytes - prevTx) / elapsed;
        auto rxRate = checkedConversion<double>(rxBytes - prevRx) / elapsed;
        txRateStr_ = byte_format::formatRate(txRate);
        rxRateStr_ = byte_format::formatRate(rxRate);
        txRate_ = txRate;
        rxRate_ = rxRate;
      }
      else
      {
        txRateStr_.clear();
        rxRateStr_.clear();
      }
      prevTransportStats_ = ts;
      prevTransportTime_ = nowSteady;
    }

    app_.setStatus(completer_.objectsInScopeCount(),
                   store_.getObjectCount(),
                   scope_.getKind() == Scope::Kind::root,
                   0U,
                   store_.getOpenSources().size(),
                   listeners_.size(),
                   scope_.getPath(),
                   lastTimeStr_,
                   txRateStr_,
                   rxRateStr_,
                   txRate_,
                   rxRate_);
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

  auto isRepl = app_.displayMode() == App::Mode::repl;

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
    if (e.desc.name == "shutdown" || (isRepl && e.desc.tuiOnly))
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
      if (e.desc.category == category && e.desc.name != "shutdown" && !(isRepl && e.desc.tuiOnly))
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
      if (e.desc.category != category || e.desc.name == "shutdown" || (isRepl && e.desc.tuiOnly))
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
  std::vector<Shortcut> shortcuts = {
    {"Tab / Shift+Tab", "Cycle through completions"},
    {"Enter / .", "Accept completion (drill into paths)"},
    {"Escape", "Cancel completion / shutdown"},
    {"Ctrl+D", "Shutdown"},
    {"Mouse drag", "Select (auto-copies on release)"},
    {"Ctrl+Y", "Copy selection to clipboard"},
  };
  if (app_.displayMode() == App::Mode::tui)
  {
    shortcuts.push_back({"F1", "Help (show command list)"});
    shortcuts.push_back({"F2", "Clear active bottom tab"});
    shortcuts.push_back({"F3", "Toggle logs tab (drag top border to resize)"});
    shortcuts.push_back({"F4", "Toggle events tab"});
    shortcuts.push_back({"F5", "Toggle watch pane (drag left border to resize)"});
  }

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

  // Validate the target against available sources when navigating into new scope levels.
  if (!isSpecial && scope_.getKind() == Scope::Kind::root && target.find('.') == std::string::npos)
  {
    auto available = store_.getAvailableSources();
    bool found = false;
    for (const auto& src: available)
    {
      if (src == target || src.compare(0, target.size() + 1, target + ".") == 0)
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      reportError("Navigation Error", "Session '" + target + "' not found. Use 'open' first.");
      return;
    }
  }

  // Auto-open bus addresses that aren't already open.
  if (!isSpecial && target.find('.') != std::string::npos)
  {
    auto busAddr = target.substr(0, target.find('/'));

    if (!store_.isSourceOpen(busAddr))
    {
      if (auto err = store_.openSource(busAddr); err.isError())
      {
        reportError("Cannot Open Source", "Failed to open '" + busAddr + "': " + err.getError());
        return;
      }
    }
  }

  if (!scope_.navigate(args))
  {
    reportError("Navigation Error", "Cannot navigate to '" + target + "' from " + std::string(scope_.getPath()));
  }
  else
  {
    completer_.markScopeDirty();
    app_.setPrompt(scope_.makePrompt());
  }
}

void CommandEngine::cmdPwd(std::string_view /*args*/) { app_.appendOutput(scope_.getPath()); }

void CommandEngine::cmdLs(std::string_view args)
{
  if (store_.getObjects().empty())
  {
    app_.appendOutput("(no objects)");
    return;
  }

  // ls @queryname: show only objects matching the named query.
  if (!args.empty() && args.front() == '@')
  {
    auto queryName = args.substr(1);
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
      auto pathComponents = impl::split(relName, '.');
      if (pathComponents.empty())
      {
        continue;
      }
      auto* node = tempTree.getOrCreateChild(pathComponents);
      std::string annotation = "[";
      annotation += obj->getClass()->getQualifiedName();
      annotation += "]";
      node->setAnnotation(std::move(annotation));
    }
    tempTree.render([this](ftxui::Element line) { app_.appendElement(std::move(line)); });
    return;
  }

  // When in query scope (cd @query), show only objects matching that query.
  if (scope_.getKind() == Scope::Kind::query && args.empty())
  {
    auto queryName = scope_.getPath();
    // getPath() for query scope returns "@queryname"; strip the @.
    if (!queryName.empty() && queryName.front() == '@')
    {
      queryName = queryName.substr(1);
    }
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
      auto pathComponents = impl::split(relName, '.');
      if (pathComponents.empty())
      {
        continue;
      }
      auto* node = tempTree.getOrCreateChild(pathComponents);
      std::string annotation = "[";
      annotation += obj->getClass()->getQualifiedName();
      annotation += "]";
      node->setAnnotation(std::move(annotation));
    }
    tempTree.render([this](ftxui::Element line) { app_.appendElement(std::move(line)); });
    return;
  }

  rebuildTreeIfNeeded();

  // If a path argument was given, render only the subtree rooted at that path.
  if (!args.empty())
  {
    auto pathComponents = impl::split(std::string(args), '.');
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
    auto pathComponents = impl::split(relName, '.');

    if (pathComponents.empty())
    {
      continue;
    }

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
    auto* parent = node->getParent();
    int depth = 0;
    {
      auto* p = node->getParent();
      while (p != nullptr && p != &cachedTree_)
      {
        ++depth;
        p = p->getParent();
      }
    }

    parent = node->getParent();
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

  std::vector<textTable::Row> tableData;
  tableData.push_back({
    {"Name", ftxui::text("Name") | ftxui::bold | styles::mutedText()},
    {"Selection", ftxui::text("Selection") | ftxui::bold | styles::mutedText()},
  });
  for (const auto& q: queries)
  {
    tableData.push_back({
      textTable::Cell(q.name),
      textTable::Cell(q.selection, highlightSelection(q.selection)),
    });
  }

  app_.appendElement(ftxui::hbox({ftxui::text("  "), textTable::render(std::move(tableData))}));
}

void CommandEngine::cmdLog(std::string_view args)
{
  if (args.empty())
  {
    // Show current level and list loggers
    auto levelName = spdlog::level::to_string_view(logRouter_.getGlobalLevel());
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
      logRouter_.setGlobalLevel(level);
      auto lvl = spdlog::level::to_string_view(level);
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

  const std::string dot = unicode::middleDot;
  reportError("Unknown Subcommand",
              "Usage: log              " + dot +
                " show loggers and levels\n"
                "       log level <lvl>  " +
                dot +
                " set global level\n"
                "       log level <name> <lvl> " +
                dot + " set logger level");
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
  // Match Ctrl+D: just flag the app. The execLoop callback in component.cpp
  // notices on the next iteration and calls requestKernelStop itself. Calling
  // requestKernelStop here would race the main thread's runner teardown
  // against the rest of this event dispatch (echoCommand epilogue, render)
  // and occasionally deadlocks on shutdown.
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
  auto timeStr = now.toLocalString();
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
  auto timeStr = now.toLocalString();
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
                                               bool ok = result.isOk();
                                               std::string marker = ok ? std::string(unicode::check) + " "
                                                                       : std::string(unicode::cross) + " ";
                                               auto color = ok ? styles::echoSuccess() : styles::echoError();
                                               auto echoElement =
                                                 ftxui::hbox({ftxui::text(marker) | ftxui::bold | color,
                                                              ftxui::paragraph(inputStr),
                                                              ftxui::filler(),
                                                              ftxui::text(timestamp) | styles::mutedText()});

                                               // Echo + result in one element so they stay together.
                                               ftxui::Elements combined;
                                               combined.push_back(echoElement);

                                               if (ok)
                                               {
                                                 if (!retType->isVoidType())
                                                 {
                                                   combined.push_back(renderMethodResult(result.getValue(), *retType));
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
                                                   errMsg = std::string(name) + ": " + e.what();
                                                 }
                                                 catch (...)
                                                 {
                                                   errMsg = std::string(name) + ": unknown error";
                                                 }
                                                 combined.push_back(renderError("Call Error", errMsg));
                                               }

                                               app_.finishPendingCall(callId, ftxui::vbox(std::move(combined)));
                                             }});
}

bool CommandEngine::tryResolveObjectMethod(std::string_view input, std::string_view cmd, std::string_view args)
{
  // Split on the last dot. Method names never contain dots, but object paths can
  auto dot = cmd.rfind('.');
  if (dot == std::string_view::npos || dot == 0 || dot == cmd.size() - 1)
  {
    return false;
  }

  auto objectName = cmd.substr(0, dot);
  auto methodName = cmd.substr(dot + 1);

  auto target = completer_.findObject(objectName);
  if (!target)
  {
    // It *looks* like an object.method call (has a dot), but the object doesn't resolve.
    // Treat this as a first-class error here rather than falling back to "Unknown Command",
    // so we can produce a helpful "did you mean?" hint against known objects.
    echoCommand(input, /*isError=*/true);
    std::string message = "No object named '" + std::string(objectName) + "' in the current scope.";
    auto hint = formatSuggestionHint(completer_.findObjectSuggestions(objectName));
    if (!hint.empty())
    {
      message += "\n" + hint;
    }
    message += "\nUse 'ls' to see visible objects.";
    reportError("Unknown Object", message);
    return true;
  }

  auto classType = target->getClass();
  const Method* method = classType->searchMethodByName(methodName);
  const Property* setterProperty = nullptr;  // non-null when method is a property setter

  // Property getter/setter methods are not in getMethods(); search via properties.
  if (method == nullptr)
  {
    auto properties = classType->getProperties(ClassType::SearchMode::includeParents);
    for (const auto& prop: properties)
    {
      if (prop->getGetterMethod().getName() == methodName)
      {
        method = &prop->getGetterMethod();
        break;
      }
      if (prop->getCategory() == PropertyCategory::dynamicRW && prop->getSetterMethod().getName() == methodName)
      {
        method = &prop->getSetterMethod();
        setterProperty = prop.get();
        break;
      }
    }
  }

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

  // Build a signature hint for error messages
  auto buildSignatureHint = [&]() -> std::string
  {
    std::string hint = std::string(objectName) + "." + std::string(methodName);
    if (methodArgs.empty())
    {
      return hint + " (no arguments)";
    }
    hint += " ";
    for (std::size_t i = 0; i < methodArgs.size(); ++i)
    {
      if (i > 0)
      {
        hint += ", ";
      }
      hint += "<" + std::string(methodArgs[i].type->getName()) + ">";
    }
    return hint;
  };

  // Parse whatever the user typed (may be fewer args than the method expects).
  VarList argValues;
  try
  {
    argValues = parseArgs(method, args);
  }
  catch (const std::exception&)
  {
    echoCommand(input, /*isError=*/true);
    reportError("Invalid arguments",
                "Could not parse '" + std::string(args) + "'. Expected format:\n  " + buildSignatureHint() +
                  "\nSeparate arguments with spaces or commas; wrap string values in double quotes.");
    return true;
  }

  if (argValues.size() > methodArgs.size())
  {
    echoCommand(input, /*isError=*/true);
    reportError("Invalid arguments",
                "Too many arguments: expected " + std::to_string(methodArgs.size()) + ", got " +
                  std::to_string(argValues.size()) + ".\nExpected format:\n  " + buildSignatureHint());
    return true;
  }

  // Adapt arguments to their expected types (catches type errors early).
  for (std::size_t i = 0; i < argValues.size(); ++i)
  {
    if (auto result = impl::adaptVariant(*methodArgs[i].type, argValues[i]); result.isError())
    {
      echoCommand(input, /*isError=*/true);
      reportError("Invalid arguments",
                  "Argument '" + methodArgs[i].name + "': " + result.getError() + "\nExpected format:\n  " +
                    buildSignatureHint());
      return true;
    }
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
                  std::to_string(argValues.size()) + ".\nExpected format:\n  " + buildSignatureHint());
    return true;
  }

  // Async invocation with pending spinner.
  invokeMethodAsync(input, methodName, target, method, std::move(argValues));
  return true;
}

}  // namespace sen::components::term
