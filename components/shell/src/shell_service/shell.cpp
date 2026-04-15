// === shell.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "shell.h"

// component
#include "completer.h"
#include "history.h"
#include "line_edit.h"
#include "printer.h"
#include "styles.h"
#include "terminal.h"
#include "terminal_tree.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_mux.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/shell.stl.h"

// std
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::shell
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] std::string getLocalNameWithoutShellPrefix(const Object* obj)
{
  const auto& result = obj->getLocalName();
  return result.substr(6);  // NOLINT 'shell.'
}

Object* getObjectByLocalName(std::string_view objectName, const ObjectList<Object>& list)
{
  const auto& objects = list.getUntypedObjects();
  for (const auto& obj: objects)
  {
    if (obj->getLocalName() == objectName)
    {
      return obj.get();
    }
  }
  return nullptr;
}

constexpr long tooManyLimit = 50U;  // NOLINT(google-runtime-int)

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Execution errors
//--------------------------------------------------------------------------------------------------------------

struct NoArgumentsException: public std::runtime_error
{
  using std::runtime_error::runtime_error;
};

struct TooManyArgsException: public std::runtime_error
{
  using std::runtime_error::runtime_error;
};

struct NotEnoughArgsException: public std::runtime_error
{
  using std::runtime_error::runtime_error;
};

struct InvalidEnumValueArg: public std::runtime_error
{
  using std::runtime_error::runtime_error;
};

struct MethodNotFound: public std::runtime_error
{
  using std::runtime_error::runtime_error;
};

struct InstanceNotFound: public std::runtime_error
{
  using std::runtime_error::runtime_error;
};

//--------------------------------------------------------------------------------------------------------------
// Util
//--------------------------------------------------------------------------------------------------------------

void adaptCallArguments(const Method* method, VarList& argValues, const Method* writerMethod)
{
  // normal call
  if (writerMethod == nullptr)
  {
    const auto& metaArgs = method->getArgs();

    // return in case the argValues don't match in size with the metaArgs
    if (argValues.size() != metaArgs.size())
    {
      return;
    }

    for (std::size_t i = 0; i < metaArgs.size(); ++i)
    {
      if (auto result = impl::adaptVariant(*metaArgs[i].type, argValues[i]); result.isError())
      {
        throw std::runtime_error(result.getError());
      }
    }

    return;
  }

  // writer method case
  VarList readerVars = argValues;
  const auto& writerArgs = writerMethod->getArgs();
  argValues.clear();
  argValues.reserve(writerArgs.size());

  for (const auto& arg: writerArgs)
  {
    argValues.emplace_back(readerVars.at(method->getArgIndexFromNameHash(arg.getNameHash())));

    if (auto result = impl::adaptVariant(*arg.type, argValues.back()); result.isError())
    {
      throw std::runtime_error(result.getError());
    }
  }
}

void parseArgv(const Method* method, std::string_view buffer, VarList& result)
{
  if (method != nullptr && !method->getArgs().empty() && method->getArgs().size() == 1U &&
      method->getArgs()[0].type->isStringType())
  {
    // special case for single strings that do not start with "
    // interpret the rest of the buffer as a string
    size_t pos = buffer.find_first_of(' ');
    if (pos != std::string::npos && buffer.size() > pos + 1U && buffer[pos + 1U] != '"')
    {
      result.emplace_back(std::string(buffer.substr(pos + 1U)));
      return;
    }
  }

  std::string remaining;

  // replace ' ' by '[' and make the last character a ']' for Json processing
  if (size_t pos = buffer.find_first_of(' '); pos != std::string::npos)
  {
    remaining = buffer.substr(pos);
    remaining[0] = '[';
    remaining.push_back(']');
  }

  trim(remaining);

  if (remaining.empty())
  {
    return;
  }

  std::string doc = "{ \"args\": ";
  doc.append(remaining);
  doc.append("}");

  result = findElement(fromJson(doc).get<VarMap>(), "args", "parsing error").get<VarList>();
}

//--------------------------------------------------------------------------------------------------------------
// ShellImpl
//--------------------------------------------------------------------------------------------------------------

ShellImpl::ShellImpl(const std::string& name,
                     kernel::RunApi& api,
                     ObjectMux& mux,
                     CustomTypeRegistry& types,
                     const Configuration& config,
                     Terminal* term)
  : ShellBase(name, config)
  , api_(api)
  , mux_(mux)
  , types_(types)
  , term_(term)
  , printer_(term_, config.bufferStyle, config.timeStyle)
{
  objects_ = std::make_unique<ObjectList<Object>>(mux_, 10U);  // NOLINT readability-magic-numbers

  std::ignore = objects_->onAdded(
    [this](const auto& addedObjects)
    {
      if (printDetectedObjects_)
      {
        auto objectCount = std::distance(addedObjects.untypedBegin, addedObjects.untypedEnd);
        if (objectCount > tooManyLimit)
        {
          term_->cprintf(informationStyle, " - shell: detected %zu new objects\n", objectCount);  // NOLINT
        }
        else
        {
          for (auto obj: addedObjects)
          {
            term_->cprint(informationStyle, " - shell: detected ");
            term_->cprint(enumValueStyle, getLocalNameWithoutShellPrefix(obj));
            term_->newLine();
          }
        }
      }
    });

  std::ignore = objects_->onRemoved(
    [this](const auto& removedObjects)
    {
      if (printDetectedObjects_)
      {
        auto objectCount = std::distance(removedObjects.untypedBegin, removedObjects.untypedEnd);
        if (objectCount > tooManyLimit)
        {
          term_->cprintf(informationStyle, " - shell: %zu objects were removed\n", objectCount);  // NOLINT
        }
        else
        {
          for (auto obj: removedObjects)
          {
            term_->cprint(informationStyle, " - shell: object removed: ");
            term_->cprint(enumValueStyle, getLocalNameWithoutShellPrefix(obj));
            term_->newLine();
          }
        }
      }
    });

  // create the line editor
  auto completer = std::make_unique<Completer>(objects_.get());
  auto history = std::make_unique<History>();
  auto inputCb = [this](auto input, kernel::RunApi& runApi) { onInput(input, runApi); };
  auto muteLogCb = [this]()
  {
    const auto& objects = logMasterSub_.list.getUntypedObjects();
    if (!objects.empty())
    {
      auto obj = objects.front();
      obj->invokeUntyped(obj->getClass()->searchMethodByName("toggleMuteAll"), {});
    }
  };

  // load the history
  try
  {
    history->load();
  }
  catch (const std::runtime_error& err)
  {
    printer_.printError(err.what());  // NOLINT
  }

  // create the line editor
  lineEdit_ = std::make_unique<LineEdit>(std::move(inputCb),
                                         std::move(muteLogCb),
                                         std::move(completer),
                                         std::move(history),
                                         term_,
                                         api,
                                         [this]() { return fetchCurrentSources(); });

  lineEdit_->setPrompt(getConfig().prompt);
  lineEdit_->setWindowTitle(api_.getAppName());
  lineEdit_->refresh();

  // open the initial sources
  for (const auto& elem: config.open)
  {
    doOpen(elem);
  }

  std::string logQuery = "SELECT sen.components.logmaster.LogMaster FROM ";
  logQuery.append(config.logBus);
  logMasterSub_.source = api.getSource(config.logBus);
  logMasterSub_.source->addSubscriber(Interest::make(logQuery, api.getTypes()), &logMasterSub_.list, true);

  // create the initial queries
  for (const auto& elem: config.query)
  {
    doQuery(elem.name, elem.selection);
  }
}

void ShellImpl::update(kernel::RunApi& runApi) { lineEdit_->processEvents(runApi); }

void ShellImpl::onInput(std::string_view input, kernel::RunApi& api)
{
  if (!printDetectedObjects_)
  {
    printDetectedObjects_ = true;
  }

  if (input.empty())
  {
    return;
  }

  lineEdit_->getHistory()->addLine(input);
  execute(input, api);
  lineEdit_->refresh();
}

ShellImpl::Call ShellImpl::parseCommand(std::string_view cmd)
{
  std::string cmdString(cmd);
  trim(cmdString);

  std::string callString;

  if (auto spacePos = cmdString.find_first_of(' '); spacePos != std::string::npos)
  {
    callString = cmdString.substr(0, spacePos);
  }
  else
  {
    callString = cmdString;
  }

  std::string objectName;
  std::string memberName;

  if (size_t pointPos = callString.find_last_of('.'); pointPos != std::string::npos)
  {
    objectName = callString.substr(0, pointPos);
    memberName = callString.substr(pointPos + 1);
  }

  Object* object = nullptr;
  const Method* method = nullptr;
  bool hasWriterSchema = false;
  const Method* writerMethod = nullptr;

  if (objectName.empty())
  {
    memberName = callString;
    object = this;
    method = object->getClass()->searchMethodByName(memberName);
  }
  else if (objects_)
  {
    // prefix 'shell.' to any object
    {
      std::string realObjectName = "shell.";
      realObjectName.append(objectName);
      objectName = realObjectName;
    }

    object = getObjectByLocalName(objectName, *objects_);
    if (object)
    {
      method = object->getClass()->searchMethodByName(memberName);

      // find the writer method if it exists
      if (const auto writerSchema = getWriterSchema(object))
      {
        hasWriterSchema = true;
        writerMethod = writerSchema.value()->searchMethodByName(memberName);
      }
    }
  }
  else
  {
    // no code needed
  }

  // parse the arguments
  Call call {cmdString, memberName, objectName, object, method, hasWriterSchema, writerMethod, {}};
  parseArgv(method, cmdString, call.args);
  return call;
}

void ShellImpl::execute(std::string_view command, kernel::RunApi& api)
{
  std::ignore = api;

  Call call;
  try
  {
    call = parseCommand(command);

    // handle the special (and fake) 'print' method
    if (call.object && call.methodName == "print")
    {
      printer_.printProperties(call.object);
      lineEdit_->refresh();
      return;
    }
    if (call.hasWriterSchema && call.writerMethod == nullptr)
    {
      printer_.printError("Method '%s' is not defined in the remote object", call.methodName.c_str());  // NOLINT
      return;
    }
    if (call.hasWriterSchema && call.writerMethod != nullptr &&
        call.method->getArgs().size() < call.writerMethod->getArgs().size())
    {
      // NOLINTNEXTLINE
      printer_.printError("Method '%s' cannot be called in the remote object, not enough args",
                          call.methodName.c_str());
      return;
    }
    if (call.method == nullptr)
    {
      printer_.printError("Method '%s' not found", call.methodName.c_str());  // NOLINT
      return;
    }
    if (call.object == nullptr)
    {
      printer_.printError("Could not find object %s", call.objectName.c_str());  // NOLINT
      return;
    }

    adaptCallArguments(call.method, call.args, call.writerMethod);
    executeCall(call, call.hasWriterSchema ? call.writerMethod : call.method);
  }
  catch (const NoArgumentsException&)
  {
    printer_.printError("The method %s takes no arguments", call.method->getName().data());  // NOLINT
    printer_.printDescription(*call.object->getClass(), call.method);
  }
  catch (const TooManyArgsException&)
  {
    printer_.printError("Too many arguments for method %s", call.method->getName().data());  // NOLINT
    printer_.printDescription(*call.object->getClass(), call.method);
  }
  catch (const NotEnoughArgsException&)
  {
    printer_.printError("Not enough arguments for method %s", call.method->getName().data());  // NOLINT
    printer_.printDescription(*call.object->getClass(), call.method);
  }
  catch (const InvalidEnumValueArg& err)
  {
    auto* argName = err.what();
    const auto* arg = call.method->getArgFromName(argName);
    if (arg)
    {
      printer_.printEnumError(argName, arg->type->asEnumType());
    }
    else
    {
      printer_.printError("Invalid enumeration '%s'", argName);  // NOLINT
    }
  }
  catch (const MethodNotFound&)
  {
    printer_.printError("Method '%s' not found", call.methodName.c_str());  // NOLINT
    printer_.printDescription(*call.object->getClass());
  }
  catch (const InstanceNotFound&)
  {
    printer_.printError("Instance '%s' not found", call.objectName.c_str());  // NOLINT
    printer_.printInstances(objects_->getObjects());
  }
  catch (const std::exception& err)
  {
    printer_.printError("%s", err.what());  // NOLINT
  }
  catch (...)
  {
    printer_.printError("Unknown exception thrown");  // NOLINT
  }
}

void ShellImpl::executeCall(const Call& call, const Method* method)
{
  const auto& command = call.command;
  const auto& argv = call.args;
  const auto& methodArgs = method->getArgs();

  if (methodArgs.empty() && !call.args.empty())
  {
    throw NoArgumentsException(method->getName().data());
  }

  if (call.args.size() > methodArgs.size())
  {
    throw TooManyArgsException(call.method->getName().data());
  }

  if (call.args.size() != methodArgs.size())
  {
    throw NotEnoughArgsException(call.method->getName().data());
  }

  // deal with property getters differently (do not trigger an actual call for a state we currently have)
  {
    auto* isGetter = std::get_if<PropertyGetter>(&call.method->getPropertyRelation());
    if (isGetter != nullptr)
    {
      printer_.printPropertyValue(isGetter->property, call.object->getPropertyUntyped(isGetter->property));
      return;
    }
  }

  call.object->invokeUntyped(method,
                             argv,
                             {this,
                              [this, method, command](const MethodResult<Var>& var)
                              {
                                printer_.printMethodCallResult(var, method, command);
                                lineEdit_->refresh();
                              }});
}

void ShellImpl::clearImpl() const { lineEdit_->clearScreen(); }

void ShellImpl::helpImpl() const
{
  auto* term = lineEdit_->getTerminal();

  auto helpTitleStyle = helpStyle;
  helpTitleStyle.flags = TextFlags::boldText | TextFlags::underlineText;

  term->newLine();
  term->print("  ");
  term->cprint(helpTitleStyle, "Shell Help\n");
  term->newLine();

  term->cprint(helpStyle, "    Welcome to the sen world :)\n");
  term->newLine();

  term->cprint(helpStyle, "    You can use [tab] to auto-complete commands\n");
  term->cprint(helpStyle, "    and arguments. Use [up] and [down] to browse\n");
  term->cprint(helpStyle, "    through the command history.\n\n");

  term->print("    ");
  term->cprint(helpTitleStyle, "Keyboard Shortcuts:\n");
  term->cprint(helpStyle, "      [ctrl + d] shuts down execution\n");
  term->cprint(helpStyle, "      [ctrl + c] kills execution\n");
  term->cprint(helpStyle, "      [F6] mutes/unmutes loggers\n");
  term->newLine();

  const ClassType& meta = *this->getClass();
  auto methods = meta.getMethods(ClassType::SearchMode::includeParents);

  if (methods.empty())
  {
    return;
  }

  term->print("    ");
  term->cprint(helpTitleStyle, "Available Commands:\n");

  std::vector<std::string> header;
  std::vector<std::vector<std::string>> table;

  header.emplace_back("");
  header.emplace_back("command");
  header.emplace_back("description");

  for (const auto& method: methods)
  {
    std::string description(method->getDescription());
    bool added = false;

    for (auto& currentRow: table)
    {
      if (currentRow[2] == description)
      {
        currentRow[1].append(", ");
        currentRow[1].append(method->getName());
        added = true;
        break;
      }
    }

    if (!added)
    {
      std::vector<std::string> row;
      row.emplace_back("-");
      row.emplace_back(method->getName());
      row.emplace_back(description);
      table.emplace_back(row);
    }
  }
  Printer::printTable(header, table, 6, false, lineEdit_->getTerminal(), helpStyle);  // NOLINT
  lineEdit_->refresh();
}

void ShellImpl::historyImpl() const
{
  const History* history = lineEdit_->getHistory();
  const auto& lines = history->getLines();

  if (!lines.empty())
  {
    lineEdit_->getTerminal()->newLine();
  }

  size_t lineNumber = 1;
  for (const auto& line: lines)
  {
    lineEdit_->getTerminal()->printf("%3zd %s\n", lineNumber, line.c_str());  // NOLINT
    lineNumber++;
  }
  lineEdit_->refresh();
}

void ShellImpl::lsImpl() const
{
  lineEdit_->getTerminal()->newLine();

  auto sessions = api_.getSessionsDiscoverer().getDetectedSources();

  if (objects_->getUntypedObjects().empty() && sessions.empty())
  {
    return;
  }

  auto instances = objects_->getUntypedObjects();
  Node root(nullptr);

  if (!instances.empty())
  {
    instances.sort([](const auto& lhs, const auto& rhs) -> bool { return lhs->getName() < rhs->getName(); });
    for (const auto& obj: instances)
    {
      auto nameComponents = ::sen::impl::split(obj->getLocalName(), '.');

      // take the "shell." prefix out of the list
      Node* node = root.getOrCreateChild(std::vector<std::string>(nameComponents.begin() + 1U, nameComponents.end()));

      // this node holds an object
      node->setHoldsObject();

      std::string postfix;
      {
        postfix.append("[");
        postfix.append(obj->getClass()->getQualifiedName());
        if (obj->asNativeObject() != nullptr)
        {
          postfix.append(", Native");
        }
        else if (obj->asProxyObject()->isRemote())
        {
          postfix.append(", Remote");
        }
        else
        {
          // no code needed
        }

        postfix.append("]");
      }

      node->setPostFix(std::move(postfix));

      // we have an object, update its parents to decorate them
      auto* thisNode = node->getParent();
      while (thisNode != nullptr)
      {
        if (thisNode->getParent() == nullptr)
        {
          // this is the root
        }
        else if (thisNode->getParent()->getParent() == nullptr)
        {
          // this is a session
          thisNode->setPostFix("[session]");
          thisNode->setStyle(sessionStyle);
        }
        else if (thisNode->getParent()->getParent()->getParent() == nullptr)
        {
          // this is a bus
          thisNode->setPostFix("[bus]");
          thisNode->setStyle(busStyle);
        }
        else
        {
          // this is a group
          if (thisNode->getHoldsObject())
          {
            thisNode->setPostFix("[object|group]");
          }
          else
          {
            thisNode->setPostFix("[group]");
          }
          thisNode->setStyle(groupStyle);
        }

        thisNode = thisNode->getParent();
      }
    }
  }

  for (const auto& sessionName: sessions)
  {
    auto itr = openSessions_.find(sessionName);
    if (itr != openSessions_.end())
    {
      auto buses = itr->second->getDetectedSources();
      for (const auto& busName: buses)
      {
        std::string addr = sessionName;
        addr.append(".");
        addr.append(busName);

        // check if we have an open bus
        if (openSources_.find(addr) == openSources_.end())
        {
          auto* node = root.getOrCreateChild({sessionName, busName}, closedSourceStyle);
          node->setPostFix("[~]");
        }
        else
        {
          std::ignore = root.getOrCreateChild({sessionName, busName});
        }
      }
    }
    else
    {
      // only the session name
      auto* node = root.getOrCreateChild({sessionName}, closedSourceStyle);
      node->setPostFix("[~]");
    }
  }

  root.print(lineEdit_->getTerminal(), "", true, true);
  lineEdit_->refresh();
}

void ShellImpl::infoImpl(const std::string& term) const
{
  if (auto type = types_.get(term))
  {
    printer_.printDescription(*type.value());
    lineEdit_->refresh();
    return;
  }

  // add the 'shell.' prefix to instance names
  std::string instanceName = "shell.";
  instanceName.append(term);

  if (const auto* instance = getObjectByLocalName(instanceName, *objects_); instance != nullptr)
  {
    printer_.printDescription(*instance);
    lineEdit_->refresh();
    return;
  }

  printer_.printError("no type or instance with '%s'", term.c_str());  // NOLINT
}

ShellImpl::SourceData& ShellImpl::getOrOpenSource(const kernel::BusAddress& address)
{
  std::string id = address.sessionName + "." + address.busName;

  // early exit if already open
  if (auto itr = openSources_.find(id); itr != openSources_.end())
  {
    return itr->second;
  }

  // ensure the session is open
  if (openSessions_.find(address.sessionName) == openSessions_.end())
  {
    openSessions_.try_emplace(address.sessionName,
                              api_.getSessionsDiscoverer().makeSessionInfoProvider(address.sessionName));
  }

  SourceData sourceData;
  sourceData.source = api_.getSource(id);
  if (!sourceData.source)
  {
    printer_.printError("could not find source");  // NOLINT
    throwRuntimeError("could not find source");
  }

  if (printDetectedObjects_)
  {
    term_->newLine();
    term_->cprint(informationStyle, " - shell opened ");
    term_->cprint(enumValueStyle, id);
    term_->newLine();
  }

  // save the reference
  openSources_.try_emplace(id, std::move(sourceData));

  return openSources_[id];
}

MaybeConstTypeHandle<ClassType> ShellImpl::getWriterSchema(const Object* object) noexcept
{
  const auto* proxy = object->asProxyObject();
  const auto* remote = proxy != nullptr ? proxy->asRemoteObject() : nullptr;
  return remote != nullptr ? remote->getWriterSchema() : std::nullopt;
}

void ShellImpl::doOpen(std::string_view term)
{
  const auto sourceName = std::string(term);

  // early exit if already open
  if (openSources_.find(sourceName) != openSources_.end())
  {
    return;
  }

  // early exit if already open
  if (openSources_.find(sourceName) != openSources_.end())
  {
    return;
  }

  auto components = impl::split(sourceName);
  if (components.size() == 1U)
  {
    // open the session if not already open. We don't need to build a source
    if (openSessions_.find(sourceName) == openSessions_.end())
    {
      term_->newLine();
      term_->cprint(informationStyle, " - shell opened ");
      term_->cprint(enumValueStyle, sourceName);
      term_->newLine();

      // only open the session
      openSessions_.try_emplace(sourceName, api_.getSessionsDiscoverer().makeSessionInfoProvider(sourceName));
    }
  }
  else if (components.size() == 2U)
  {
    if (components.at(0).empty() || components.at(1).empty())
    {
      throwRuntimeError("invalid source");
    }

    auto& sourceData = getOrOpenSource(kernel::BusAddress {components.front(), components.back()});

    const std::string providerName = sourceName + ".all";

    if (sourceData.providers.find(providerName) != sourceData.providers.end())
    {
      throwRuntimeError("already open");
    }

    ProviderData providerData;
    providerData.interest = Interest::make("SELECT * FROM " + sourceName, api_.getTypes());
    providerData.provider = sourceData.source->getOrCreateNamedProvider(providerName, providerData.interest);
    providerData.provider->addListener(&mux_, true);

    sourceData.providers.try_emplace(providerName, std::move(providerData));
  }
  else
  {
    throwRuntimeError("invalid source");
  }
}

void ShellImpl::openImpl(const std::string& source) { doOpen(source); }

void ShellImpl::doQuery(std::string_view name, std::string_view selection)
{
  // validate the name
  if (name.find_first_of(' ') != std::string::npos || name.find_first_of('.') != std::string::npos)
  {
    throwRuntimeError("name cannot contain spaces or dots");
  }

  // build the interest
  auto interest = Interest::make(selection, api_.getTypes());

  // ensure the interest defines the bus
  if (!interest->getBusCondition().has_value())
  {
    throwRuntimeError("invalid source specification");
  }

  // get the bus
  const auto& busCondition = interest->getBusCondition().value();
  auto& sourceData = getOrOpenSource(kernel::BusAddress {busCondition.sessionName, busCondition.busName});

  std::string providerName = busCondition.sessionName + "." + busCondition.busName + ".";
  providerName.append(name);

  // check that the query is not repeated
  for (const auto& [queryName, queryData]: sourceData.providers)
  {
    if (queryName == providerName)
    {
      throwRuntimeError("repeated query name");
    }

    if (queryData.interest->getQueryString() == selection)
    {
      std::string err;
      err.append("a query named '");
      err.append(queryName);
      err.append("' already exists with the same definition");
      throwRuntimeError(err);
    }
  }

  ProviderData providerData;
  providerData.interest = interest;
  providerData.provider = sourceData.source->getOrCreateNamedProvider(providerName, providerData.interest);
  providerData.provider->addListener(&mux_, true);

  sourceData.providers.try_emplace(providerName, std::move(providerData));
}

void ShellImpl::queryImpl(const std::string& name, const std::string& condition) { doQuery(name, condition); }

void ShellImpl::srcImpl()
{
  if (openSources_.empty())
  {
    return;
  }

  term_->newLine();
  std::vector<std::vector<std::string>> content;
  for (const auto& [srcName, srcData]: openSources_)
  {
    for (const auto& [providerName, providerData]: srcData.providers)
    {
      content.push_back({providerName, providerData.interest->getQueryString()});
    }
  }

  Printer::printTable({"Name", "Query"}, content, 2U, true, term_);
}

void ShellImpl::closeImpl(const std::string& source)
{
  auto components = impl::split(source);
  std::vector<std::string> sourcesToClose;

  const auto& sessionName = components.front();

  if (components.size() == 1U)
  {
    // close the session discoverer
    openSessions_.erase(sessionName);

    // close all the open buses
    for (const auto& [busAddress, sourceData]: openSources_)
    {
      auto busComponents = impl::split(busAddress);
      if (busComponents.front() == sessionName)
      {
        sourcesToClose.push_back(busAddress);
      }
    }
  }
  else if (components.size() == 2U)
  {
    // close the bus
    auto itr = openSources_.find(source);
    if (itr != openSources_.end())
    {
      sourcesToClose.push_back(itr->first);
    }
  }
  else if (components.size() == 3U)
  {
    std::string sourceName = components.at(0) + "." + components.at(1);
    auto sourceItr = openSources_.find(sourceName);
    if (sourceItr == openSources_.end())
    {
      throwRuntimeError(sourceName + " is not open");
    }

    auto providerItr = sourceItr->second.providers.find(source);
    if (providerItr == sourceItr->second.providers.end())
    {
      throwRuntimeError(source + " not found");
    }

    sourceItr->second.source->removeNamedProvider(source);
    providerItr->second.provider->removeListener(&mux_, true);
    sourceItr->second.providers.erase(providerItr);

    if (!sourceItr->second.providers.empty())
    {
      return;
    }

    sourcesToClose.push_back(sourceName);
  }
  else
  {
    throwRuntimeError("invalid source name");
  }

  bool firstTime = true;
  for (const auto& elem: sourcesToClose)
  {
    auto itr = openSources_.find(elem);
    if (itr == openSources_.end())
    {
      continue;
    }

    for (const auto& [providerName, providerData]: itr->second.providers)
    {
      itr->second.source->removeNamedProvider(providerName);
      providerData.provider->removeListener(&mux_, true);
    }

    if (firstTime)
    {
      firstTime = false;
      term_->newLine();
    }

    if (printDetectedObjects_)
    {
      term_->cprint(informationStyle, " - shell closed ");
      term_->cprint(enumValueStyle, itr->first);
      term_->newLine();
    }

    openSources_.erase(itr);
  }
}

void ShellImpl::shutdownImpl() const
{
  term_->cprint(informationStyle, "shutting down... ");
  api_.requestKernelStop(0);
}

std::vector<std::string> ShellImpl::fetchCurrentSources()
{
  auto sessions = api_.getSessionsDiscoverer().getDetectedSources();
  std::vector<std::string> result;

  for (const auto& sessionName: sessions)
  {
    auto itr = openSessions_.find(sessionName);
    if (itr != openSessions_.end())
    {
      auto buses = itr->second->getDetectedSources();
      for (const auto& busName: buses)
      {
        result.emplace_back();
        result.back().append(sessionName);
        result.back().append(".");
        result.back().append(busName);
      }
    }
    else
    {
      result.push_back(sessionName);
    }
  }

  return result;
}

}  // namespace sen::components::shell
