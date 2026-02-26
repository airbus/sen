// === bootloader.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/bootloader.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"

// generated code
#include "sen/kernel/kernel_config.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/log.stl.h"

// c4
#include "c4/charconv.hpp"
#include "c4/std/string.hpp"
#include "c4/substr.hpp"
#include "c4/substr_fwd.hpp"
#include "c4/yml/node.hpp"
#include "c4/yml/parse.hpp"
#include "c4/yml/tree.hpp"
#include "c4/yml/yml.hpp"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::kernel
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr auto defaultPipelineFreqHz = 30.0f;
constexpr auto defaultPipelinePeriod = Duration::fromHertz(defaultPipelineFreqHz);
constexpr uint64_t defaultMaxQueueSize = 0;
constexpr auto defaultQueueEvictionPolicy = QueueEvictionPolicy::dropNewest;
constexpr auto defaultLogLevel = log::LogLevel::info;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

void readFile(const std::filesystem::path& fileName, std::string& contents)
{
  std::ifstream in(fileName);

  if (!in.is_open())
  {
    std::string err;
    err.append("error opening file '");
    err.append(fileName.generic_string());
    err.append("'");
    throwRuntimeError(err);
  }

  // reserve required memory in one go
  in.seekg(0U, std::ios::end);
  contents.reserve(static_cast<std::size_t>(in.tellg()));
  in.seekg(0U, std::ios::beg);

  // read contents
  contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

template <typename T>
[[nodiscard]] T searchChildOr(const VarMap& map, const std::string& name, T defaultVal)
{
  auto itr = map.find(name);
  if (itr == map.end())
  {
    return defaultVal;
  }
  return getCopyAs<T>(itr->second);
}

[[nodiscard]] Var yamlValueToVar(c4::csubstr yaml, bool isQuoted)
{
  if (isQuoted)
  {
    std::string val;
    c4::from_chars(yaml, &val);
    return val;
  }

  if (yaml.is_integer())
  {
    int64_t val = 0;
    c4::from_chars(yaml, &val);
    return val;
  }
  if (yaml.is_unsigned_integer())
  {
    uint64_t val = 0;
    c4::from_chars(yaml, &val);
    return val;
  }
  if (yaml.is_real())
  {
    float64_t val = 0.0;
    c4::from_chars(yaml, &val);
    return val;
  }

  std::string val;
  c4::from_chars(yaml, &val);
  return val;
}

[[nodiscard]] Var yamlToVar(c4::yml::ConstNodeRef node)
{
  if (node.empty())
  {
    return {};
  }

  if (node.is_map())
  {
    VarMap map;
    for (const auto& child: node.cchildren())
    {
      std::string keyStr;
      c4::from_chars(child.key(), &keyStr);

      if (child.is_keyval())
      {
        map.try_emplace(keyStr, yamlValueToVar(child.val(), node.is_val_quoted()));
      }
      else
      {
        map.try_emplace(keyStr, yamlToVar(child));
      }
    }
    return map;
  }

  if (node.is_seq())
  {
    VarList list;
    list.reserve(node.num_children());

    for (const auto& child: node.cchildren())
    {
      list.push_back(yamlToVar(child));
    }
    return list;
  }

  if (node.is_val())
  {
    return yamlValueToVar(node.val(), node.is_quoted());
  }

  std::string err;
  err.append("unexpected node of type ");
  err.append(node.type_str());
  throwRuntimeError(err);
}

void importYaml(c4::yml::Tree& into, c4::yml::ConstNodeRef from, const std::filesystem::path& path)
{
  std::string fileString;
  c4::from_chars(from.val(), &fileString);

  std::string fileContents;
  readFile(path.parent_path() / fileString, fileContents);

  c4::substr fileData = into.alloc_arena(fileContents.size());
  fileData.copy_from(c4::to_csubstr(fileContents));
  c4::yml::Tree fileTree = c4::yml::parse_in_place(fileData);

  into.merge_with(&fileTree, fileTree.root_id(), into.root_id());

  // move the newly-included node to the top of the tree (ensures correct overwrites)
  if (const auto newNode = fileTree.crootref(); newNode.is_map())
  {
    if (newNode.find_child("load").valid())
    {
      into.rootref()["load"].last_child().move({});
    }
    if (newNode.find_child("build").valid())
    {
      into.rootref()["build"].last_child().move({});
    }
  }
}

[[nodiscard]] bool isKeyedSequence(c4::yml::NodeRef node, std::string_view key)
{
  if (!node.valid() || !node.is_seq())
  {
    return false;
  }

  auto keyString = c4::to_csubstr(key.data());

  for (auto sequenceElement: node)
  {
    if (!sequenceElement.valid() || !sequenceElement.is_map() || !sequenceElement.has_child(keyString))
    {
      return false;
    }
  }

  return true;
}

void combineRepeatedElements(c4::yml::NodeRef node, c4::yml::Tree* tree, std::string_view key)
{
  auto keyString = c4::to_csubstr(key.data());

  // combine only if the node is a sequence of maps which contain a key
  if (isKeyedSequence(node, key))
  {
    std::map<std::string, c4::yml::NodeRef> nodeMap;

    for (auto sequenceElement: node)
    {
      // get the key
      std::string keyValue;
      c4::from_chars(sequenceElement[keyString].val(), &keyValue);

      // see if we already have a node with this key
      auto itr = nodeMap.find(keyValue);
      if (itr == nodeMap.end())
      {
        // if not, simply ignore it
        nodeMap.try_emplace(keyValue, sequenceElement);
      }
      else
      {
        // if we do, combine it and mark the other node for deletion
        tree->merge_with(sequenceElement.tree(), sequenceElement.id(), itr->second.id());

        auto deleteNode = sequenceElement["sen_temp_delete_node"];
        deleteNode |= c4::yml::VAL;
        deleteNode.set_val("true");
      }
    }

    bool removed = true;
    while (removed)
    {
      removed = false;
      for (auto sequenceElement: node)
      {
        if (sequenceElement.has_child("sen_temp_delete_node"))
        {
          node.remove_child(sequenceElement);
          removed = true;
          break;
        }
      }
    }
  }

  // continue traversing the nodes
  for (auto child: node)
  {
    combineRepeatedElements(child, child.tree(), key);
  }
}

[[nodiscard]] std::string replaceEnvPattern(const std::string& content)
{
  std::string expandedContent = content;
  std::regex pattern(R"((\\*)@env\(([a-zA-Z_$][\w]*)(,([\w]+))?\))");

  auto begin = std::sregex_iterator(expandedContent.begin(), expandedContent.end(), pattern);
  auto end = std::sregex_iterator();

  std::string result;
  std::string::const_iterator last = expandedContent.begin();

  for (auto i = begin; i != end; ++i)
  {
    const std::smatch& match = *i;
    result.append(last, match[0].first);
    last = match[0].second;

    // Extract the environment variable name
    std::string envVar = match[2].str();

    // Get the environment variable value
    const char* envValue = std::getenv(envVar.c_str());
    std::string value;

    if (envValue)
    {
      value = std::string(envValue);
    }
    else if (match[4].matched)
    {
      // Use the default value from the second group if the environment variable is not found
      value = match[4].str();
    }
    else
    {
      // Replace with VARIABLE_NOT_FOUND string
      throwRuntimeError("environment variable " + envVar + " not found and no default value provided");
    }

    // Count the number of backslashes before the match
    std::string backslashes = match[1].str();
    size_t backslashCount = backslashes.size();
    auto halvedBackslashCount = static_cast<int>(std::round(backslashCount * 0.5));  // NOLINT
    // If there are an even number of backslashes, halve them and resolve the variable
    if (backslashCount % 2 == 0)
    {
      if (backslashCount == 0)
      {
        result += value;
      }
      else
      {
        result += std::string(halvedBackslashCount, '\\') + value;
      }
    }
    else
    {
      // If there are an odd number of backslashes, replace the last backslash with the @env pattern
      result += std::string(halvedBackslashCount - 1, '\\') + "@env(" + envVar;
      if (match[4].matched)
      {
        result += "," + match[4].str();
      }
      result += ")";
    }
  }

  // Ensure both iterators are of the same type
  result.append(last, expandedContent.cend());
  return result;
}

[[nodiscard]] c4::yml::Tree loadYamlFromString(const std::string& content, const std::filesystem::path& path)
{
  // Use std::regex_replace to perform the replacement
  const std::string expandedContent = replaceEnvPattern(content);
  c4::yml::Tree root = c4::yml::parse_in_arena(c4::to_csubstr(expandedContent));

  bool hasIncludes = false;
  if (auto rootNode = root.crootref(); rootNode.is_map())
  {
    if (auto child = rootNode.find_child("include"); child.valid())
    {
      hasIncludes = true;
      if (child.is_keyval())
      {
        importYaml(root, child, path);
      }
      else if (child.is_seq())
      {
        for (auto elem: child)
        {
          if (!elem.is_val())
          {
            throwRuntimeError("invalid include (expecting file name)");
          }
          importYaml(root, elem, path);
        }
      }
      else
      {
        throwRuntimeError("invalid include");
      }
    }
  }

  if (hasIncludes)
  {
    root.rootref().remove_child("include");
    combineRepeatedElements(root.rootref(), &root, "name");
  }

  return root;
}

[[nodiscard]] QueueConfig getDefaultQueueConfig() { return {defaultQueueEvictionPolicy, defaultMaxQueueSize}; }

[[nodiscard]] kernel::log::Config getDefaultLogConfig()
{
  kernel::log::Config config {};
  config.level = defaultLogLevel;
  config.backtrace = false;
  config.pattern = {};

  return config;
}

[[nodiscard]] kernel::KernelParams getDefaultKernelParams()
{
  kernel::KernelParams params {};
  params.clockMaster = false;
  params.runMode = kernel::RunMode::realTime;
  params.logConfig = getDefaultLogConfig();
  params.sleepPolicy = SystemSleep {};
  return params;
}

void setDefaultComponentConfig(ComponentConfig& config)
{
  config.priority = Priority::nominalMin;
  config.stackSize = 0U;
  config.group = 1U;
  config.inQueue = getDefaultQueueConfig();
  config.outQueue = getDefaultQueueConfig();
  config.sleepPolicy = SystemSleep {};
}

[[nodiscard]] ComponentConfig getDefaultComponentConfig()
{
  ComponentConfig config {};
  setDefaultComponentConfig(config);
  return config;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Free functions
//--------------------------------------------------------------------------------------------------------------

VarMap getConfigAsVarFromYaml(const std::filesystem::path& path, bool printFinalConfig)
{
  std::string content;
  readFile(path, content);
  return getConfigAsVarFromYaml(content, path, printFinalConfig);
}

VarMap getConfigAsVarFromYaml(const std::string& content, const std::filesystem::path& path, bool printFinalConfig)
{
  auto root = loadYamlFromString(content, path);

  // resolve anchors and aliases on configuration file
  root.resolve();

  if (printFinalConfig)
  {
    c4::yml::emit_yaml(root, root.root_id(), stdout);
  }

  auto var = yamlToVar(root.crootref());

  if (var.isEmpty())
  {
    var = VarMap {};
  }

  if (!var.holds<VarMap>())
  {
    std::string err;
    err.append("invalid configuration in ");
    err.append(path.generic_string());
    throwRuntimeError(err);
  }

  return var.get<VarMap>();
}

//--------------------------------------------------------------------------------------------------------------
// Bootloader
//--------------------------------------------------------------------------------------------------------------

Bootloader::Bootloader() = default;

Bootloader::~Bootloader() = default;

std::unique_ptr<Bootloader> Bootloader::fromYamlFile(const std::filesystem::path& path, bool printConfig)
{
  std::unique_ptr<Bootloader> bootloader(new Bootloader());  // NOSONAR
  bootloader->loadConfigFromFile(path, printConfig);
  return bootloader;
}

std::unique_ptr<Bootloader> Bootloader::fromYamlString(const std::string& config, bool printConfig)
{
  std::unique_ptr<Bootloader> bootloader(new Bootloader());  // NOSONAR
  bootloader->loadConfigFromString(config, {}, printConfig);
  return bootloader;
}

void Bootloader::loadConfigFromString(const std::string& content,
                                      const std::filesystem::path& path,
                                      bool printFinalConfig)
{
  // save the config
  configVar_ = getConfigAsVarFromYaml(content, path, printFinalConfig);

  // load the config
  loadKernelConfig(configVar_);
  loadComponents(configVar_);
  loadPipelines(configVar_);
}

void Bootloader::loadConfigFromFile(const std::filesystem::path& path, bool printFinalConfig)
{
  std::string content;
  readFile(path, content);
  loadConfigFromString(content, path, printFinalConfig);
  config_.setConfigFilePath(path);
}

KernelConfig& Bootloader::getConfig() noexcept { return config_; }

void Bootloader::loadKernelConfig(const VarMap& data)
{
  auto kernelParamsVar = searchChildOr<VarMap>(data, "kernel", {});
  auto params = getDefaultKernelParams();

  VariantTraits<kernel::KernelParams>::variantToValue(kernelParamsVar, params);

  if (params.clockBus.empty())
  {
    params.clockBus = params.bus;
  }

  config_.setParams(params);
}

void Bootloader::loadComponents(const VarMap& data)
{
  // early exit
  if (data.empty())
  {
    return;
  }

  // to track repeated
  std::set<std::string, std::less<>> toLoad;

  // fetch components to load
  auto components = searchChildOr<VarList>(data, "load", {});
  for (const auto& elem: components)
  {
    auto componentData = elem.getCopyAs<VarMap>();
    const auto nameVar = findElement(componentData, "name", "components need a name");
    auto componentName = nameVar.getCopyAs<std::string>();

    // complain if repeated
    if (toLoad.count(componentName) != 0)
    {
      std::string err;
      err.append("component '");
      err.append(componentName);
      err.append("' is repeated in the load section");
      throwRuntimeError(err);
    }

    component(componentName, componentData);
    toLoad.insert(componentName);
  }
}

void Bootloader::loadPipelines(const VarMap& data)
{
  // to track repeated
  std::set<std::string, std::less<>> toBuild;

  auto pipelines = searchChildOr<VarList>(data, "build", {});
  for (const auto& elem: pipelines)
  {
    auto pipelineData = elem.getCopyAs<VarMap>();
    const auto nameVar = findElement(pipelineData, "name", "pipelines need a name");
    auto pipelineName = nameVar.getCopyAs<std::string>();

    if (toBuild.count(pipelineName) != 0)
    {
      std::string err;
      err.append("component '");
      err.append(pipelineName);
      err.append("' is repeated in the build section");
      throwRuntimeError(err);
    }

    pipeline(pipelineName, pipelineData);
    toBuild.insert(pipelineName);
  }
}

void Bootloader::component(std::string_view path, const Var& data)
{
  // save the configuration of the component into
  // its own key and plug the component
  const auto map = getCopyAs<VarMap>(data);

  auto config = getDefaultComponentConfig();
  VariantTraits<ComponentConfig>::variantToValue(data, config);

  config_.addToLoad(KernelConfig::ComponentPluginToLoad {std::string(path), config, map});
}

void Bootloader::pipeline(std::string_view name, const Var& data)
{
  const auto map = getCopyAs<VarMap>(data);

  KernelConfig::PipelineToLoad pipelineConfig;
  pipelineConfig.name = name;
  pipelineConfig.imports = findElementAs<StringList>(map, "imports", "missing imports");

  fetchPipelineObjects(pipelineConfig, map);
  fetchPipelineExecPeriod(pipelineConfig, map);
  fetchPipelineComponentConfig(pipelineConfig, data);
  fetchPipelineUserDefinedParams(pipelineConfig, map);

  config_.addToLoad(std::move(pipelineConfig));
}

void Bootloader::fetchPipelineObjects(KernelConfig::PipelineToLoad& pipelineConfig, const VarMap& map)
{
  // look for objects
  auto objectsItr = map.find("objects");
  if (objectsItr == map.end())
  {
    std::string err;
    err.append("no objects defined for pipeline '");
    err.append(pipelineConfig.name);
    err.append("'");
    throwRuntimeError(err);
  }

  // to track repeated
  std::set<std::string, std::less<>> toInstantiate;

  const auto objList = getCopyAs<VarList>(objectsItr->second);
  for (const auto& elem: objList)
  {
    const auto& objParams = getCopyAs<VarMap>(elem);
    KernelConfig::ObjectConfig objConfig;
    objConfig.name = findElementAs<std::string>(objParams, "name", "missing object name");
    objConfig.className = findElementAs<std::string>(objParams, "class", "missing class name");
    objConfig.params = objParams;

    if (toInstantiate.count(objConfig.name) != 0)
    {
      std::string err;
      err.append("repeated object name '");
      err.append(objConfig.name);
      err.append("' in component '");
      err.append(pipelineConfig.name);
      err.append("'");
      throwRuntimeError(err);
    }

    if (const auto busItr = objParams.find("bus"); busItr != objParams.end())
    {
      auto busString = busItr->second.getCopyAs<std::string>();
      auto busElements = ::sen::impl::split(busString, '.');
      if (busElements.size() != 2U)
      {
        std::string err;
        err.append("invalid bus address '");
        err.append(busString);
        err.append("'");
        throwRuntimeError(err);
      }
      objConfig.bus.sessionName = busElements.at(0U);
      objConfig.bus.busName = busElements.at(1U);
    }

    pipelineConfig.objects.push_back(std::move(objConfig));
  }
}

void Bootloader::fetchPipelineComponentConfig(KernelConfig::PipelineToLoad& pipelineConfig, const Var& data)
{
  setDefaultComponentConfig(pipelineConfig.config);
  pipelineConfig.config.sleepPolicy = PrecisionSleep {};

  VariantTraits<ComponentConfig>::variantToValue(data, pipelineConfig.config);

  if (pipelineConfig.period.getNanoseconds() == 0)
  {
    SPDLOG_WARN("component frequency not set. Defaulting to {} Hz", defaultPipelineFreqHz);
    pipelineConfig.period = defaultPipelinePeriod;
  }
}

void Bootloader::fetchPipelineExecPeriod(KernelConfig::PipelineToLoad& pipelineConfig, const VarMap& map)
{
  auto freqItr = map.find("freqHz");
  if (freqItr != map.end())
  {
    const auto freqVal = getCopyAs<float64_t>(freqItr->second);
    if (freqVal == 0.0)
    {
      std::string err;
      err.append("invalid frequency value '");
      err.append(std::to_string(freqVal));
      err.append("' for pipeline '");
      err.append(pipelineConfig.name);
      err.append("'");
      throwRuntimeError(err);
    }

    pipelineConfig.period = Duration::fromHertz(freqVal);
  }
}

void Bootloader::fetchPipelineUserDefinedParams(KernelConfig::PipelineToLoad& pipelineConfig, const VarMap& map)
{
  auto params = VarMap();
  for (auto& [key, var]: map)
  {
    if (key == "group" || key == "objects" || key == "imports")
    {
      continue;
    }
    params.try_emplace(key, var);
  }

  pipelineConfig.params = params;
}

}  // namespace sen::kernel
