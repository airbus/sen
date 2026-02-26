// === completer.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "completer.h"

// component
#include "styles.h"
#include "terminal.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/shell.stl.h"

// std
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::shell
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

// Forward declarations
class MetaLeaf;
class MetaGroup;

inline bool startsWith(std::string_view str, std::string_view prefix) noexcept
{
  const size_t len = prefix.length();
  return (len != 0U) && (len <= str.length()) && (str.compare(0, len, prefix) == 0);
}

//--------------------------------------------------------------------------------------------------------------
// MetaNode
//--------------------------------------------------------------------------------------------------------------

class MetaNode
{
public:
  SEN_MOVE_ONLY(MetaNode)

public:
  MetaNode() = default;
  virtual ~MetaNode() = default;

public:
  [[nodiscard]] virtual MetaGroup* asMetaGroup() noexcept { return nullptr; }

  [[nodiscard]] virtual MetaLeaf* asMetaLeaf() noexcept { return nullptr; }

  [[nodiscard]] const std::string& getCompletePath() const noexcept { return completePath_; }

  void setCompletePath(std::string_view path) { completePath_ = path; }

private:
  std::string completePath_;
};

//--------------------------------------------------------------------------------------------------------------
// MetaLeaf
//--------------------------------------------------------------------------------------------------------------

class MetaLeaf: public MetaNode
{
public:
  SEN_MOVE_ONLY(MetaLeaf)

public:
  MetaLeaf() = default;
  ~MetaLeaf() override = default;

public:
  [[nodiscard]] MetaGroup* asMetaGroup() noexcept override { return nullptr; }

  [[nodiscard]] MetaLeaf* asMetaLeaf() noexcept override { return this; }

  [[nodiscard]] const Method* getMethod() const noexcept { return method_; }

  void setMethod(const Method* method) noexcept { method_ = method; }

  void setInstance(const Object* instance) noexcept { instance_ = instance; }

private:
  const Method* method_ = nullptr;
  const Object* instance_ = nullptr;
};

//--------------------------------------------------------------------------------------------------------------
// SearchResult
//--------------------------------------------------------------------------------------------------------------

struct SearchResult
{
  MetaNode* node = nullptr;
  std::string completion;
};

//--------------------------------------------------------------------------------------------------------------
// Containers
//--------------------------------------------------------------------------------------------------------------

using SearchResultList = std::list<SearchResult>;
using MetaNodeMap = std::map<std::string, std::unique_ptr<MetaNode>, std::less<>>;

//--------------------------------------------------------------------------------------------------------------
// MetaGroup
//--------------------------------------------------------------------------------------------------------------

class MetaGroup: public MetaNode
{
public:
  SEN_MOVE_ONLY(MetaGroup)

public:
  MetaGroup() = default;
  ~MetaGroup() override = default;

public:
  [[nodiscard]] MetaGroup* asMetaGroup() noexcept override { return this; }

  [[nodiscard]] MetaLeaf* asMetaLeaf() noexcept override { return nullptr; }

public:
  [[nodiscard]] bool isObject() const noexcept { return isObject_; }

  void setIsObject(bool isObject) noexcept { isObject_ = isObject; }

  void addChild(const std::string& name, std::unique_ptr<MetaNode>&& child)
  {
    children_.try_emplace(name, std::move(child));
  }

  [[nodiscard]] MetaGroup* getOrCreateChildMetaGroup(const std::string& name)
  {
    if (auto itr = children_.find(name); itr != children_.end())
    {
      return itr->second->asMetaGroup();
    }

    auto [place, done] = children_.try_emplace(name, std::make_unique<MetaGroup>());
    return place->second->asMetaGroup();
  }

  void findChildrenStartingWith(std::string_view prefix, SearchResultList& result)
  {
    auto pointPos = prefix.find_first_of('.');

    if (pointPos == std::string::npos)
    {
      for (const auto& [name, node]: children_)
      {
        if (prefix.empty() || startsWith(name, prefix))
        {
          SearchResult r;
          r.completion = name.substr(prefix.length());
          r.node = node.get();

          result.push_back(std::move(r));
        }
      }
    }
    else
    {
      auto preSeparator = prefix.substr(0, pointPos);
      auto postSeparator = prefix.substr(pointPos + 1);

      if (auto itr = children_.find(preSeparator); itr != children_.end())
      {
        if (MetaGroup* groupChild = itr->second->asMetaGroup())
        {
          groupChild->findChildrenStartingWith(postSeparator, result);
        }
      }
    }
  }

  [[nodiscard]] const MetaNodeMap& getChildren() const noexcept { return children_; }

private:
  MetaNodeMap children_;
  bool isObject_ = false;
};

[[nodiscard]] std::string computeInstanceName(const Object* instance)
{
  const auto& localName = instance->getLocalName();
  auto firstDot = localName.find_first_of('.');
  return localName.substr(firstDot + 1U);
}

[[nodiscard]] std::string computePath(Object* instance, const Method* method)
{
  auto instanceName = computeInstanceName(instance);

  std::string path;
  path.reserve(instanceName.size() + method->getName().size() + 3U);
  path.append(instanceName);
  path.append(".");
  path.append(method->getName());

  return path;
}

[[nodiscard]] MetaGroup* getOrCreateParentGroup(MetaGroup* root, const std::string& path)
{
  auto groups = ::sen::impl::split(path, '.');

  MetaGroup* parent = root;

  if (!groups.empty())
  {
    std::string currentPath;
    for (unsigned int i = 0; i < groups.size() - 1; ++i)
    {
      currentPath += groups[i];
      parent = parent->getOrCreateChildMetaGroup(groups[i]);
      parent->setCompletePath(currentPath);

      if (i != (groups.size() - 1))
      {
        currentPath += ".";
      }
    }
  }

  return parent;
}

[[nodiscard]] std::unique_ptr<MetaGroup> createRoot(const std::list<Object*>& instances)
{
  auto root = std::make_unique<MetaGroup>();

  // add root methods
  {
    const auto& meta = MetaTypeTrait<ShellInterface>::meta();
    const auto methods(meta->getMethods(ClassType::SearchMode::includeParents));

    for (const auto& method: methods)
    {
      auto leaf = std::make_unique<MetaLeaf>();
      leaf->setInstance(nullptr);
      leaf->setMethod(method.get());
      leaf->setCompletePath(method->getName());
      root->addChild(std::string(method->getName()), std::move(leaf));
    }
  }

  // add instances
  for (auto* instance: instances)
  {
    auto instanceName = computeInstanceName(instance);
    auto meta = instance->getClass();
    const auto methods = meta->getMethods(ClassType::SearchMode::includeParents);

    MetaGroup* objectGroup = nullptr;

    for (const auto& method: methods)
    {
      auto path = computePath(instance, method.get());

      if (objectGroup == nullptr)
      {
        objectGroup = getOrCreateParentGroup(root.get(), path);
      }

      {
        auto leaf = std::make_unique<MetaLeaf>();
        leaf->setInstance(instance);
        leaf->setMethod(method.get());
        leaf->setCompletePath(path);
        objectGroup->addChild(std::string(method->getName()), std::move(leaf));
      }
    }

    const auto props = meta->getProperties(ClassType::SearchMode::includeParents);
    for (const auto& prop: props)
    {
      const auto* getter = &prop->getGetterMethod();
      const auto* setter = &prop->getSetterMethod();

      auto getterPath = computePath(instance, getter);
      auto setterPath = computePath(instance, setter);

      if (objectGroup == nullptr)
      {
        objectGroup = getOrCreateParentGroup(root.get(), getterPath);
      }

      // getter
      {
        auto leaf = std::make_unique<MetaLeaf>();
        leaf->setInstance(instance);
        leaf->setMethod(getter);
        leaf->setCompletePath(getterPath);
        objectGroup->addChild(std::string(getter->getName()), std::move(leaf));
      }

      // setter
      if (prop->getCategory() == PropertyCategory::dynamicRW)
      {
        auto leaf = std::make_unique<MetaLeaf>();
        leaf->setInstance(instance);
        leaf->setMethod(setter);
        leaf->setCompletePath(setterPath);
        objectGroup->addChild(std::string(setter->getName()), std::move(leaf));
      }
    }

    // fake method 'print'
    {
      std::string completePath;
      completePath.append(instanceName);
      completePath.append(".print");

      if (objectGroup == nullptr)
      {
        objectGroup = getOrCreateParentGroup(root.get(), completePath);
      }

      auto leaf = std::make_unique<MetaLeaf>();
      leaf->setInstance(instance);
      leaf->setMethod(nullptr);
      leaf->setCompletePath(completePath);

      objectGroup->addChild("print", std::move(leaf));
    }

    objectGroup->setIsObject(true);
  }

  return root;
}

void computeCommonPrefix(const std::string& cmd, CompletionList& result, std::string& commonPrefix)
{
  commonPrefix.clear();
  if (cmd.empty())
  {
    return;
  }

  for (const auto& completion: result)
  {
    const auto& value = cmd + completion.get();

    if (commonPrefix.empty())
    {
      commonPrefix = value;
    }
    else
    {
      std::string lhs;
      std::string rhs;

      if (value.length() < commonPrefix.length())
      {
        lhs = value;
        rhs = commonPrefix;
      }
      else
      {
        lhs = commonPrefix;
        rhs = value;
      }

      commonPrefix.clear();
      for (unsigned int i = 0; i < lhs.length(); ++i)
      {
        if (lhs[i] != rhs[i])
        {
          break;
        }

        commonPrefix += lhs[i];
      }
    }
  }
}

void removeDuplicates(CompletionList& result)
{
  CompletionList temp;
  for (const auto& completion: result)
  {
    bool found = false;

    for (const auto& elem: temp)
    {
      if (elem.get() == completion.get())
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      temp.push_back(completion);
    }
  }
  result = temp;
}

//--------------------------------------------------------------------------------------------------------------
// Completion logic
//--------------------------------------------------------------------------------------------------------------

void completeOnEmptyCommand(const MetaGroup* root, CompletionList& result)
{
  for (const auto& [name, node]: root->getChildren())
  {
    Completion completion;
    completion.set(name);

    if (node->asMetaGroup())
    {
      if (node->asMetaGroup()->isObject())
      {
        completion.configureAsObject();
      }
      else
      {
        completion.configureAsGroup();
      }
    }
    else
    {
      completion.configureAsMethod();
    }

    result.push_back(completion);
  }
}

void completeFromTree(MetaGroup* root, std::string_view baseCmd, CompletionList& list)
{
  SearchResultList children;
  root->findChildrenStartingWith(baseCmd, children);

  // check for single group that can be expanded
  if (children.size() == 1U && (children.begin()->node->asMetaGroup() != nullptr))
  {
    Completion completion;
    std::string str;
    str = children.begin()->node->getCompletePath().substr(baseCmd.length());
    str.append(".");
    completion.set(str);
    completion.configureAsObject();

    list.push_back(std::move(completion));
  }
  else
  {
    for (const auto& child: children)
    {
      Completion completion;
      completion.set(child.completion);

      const auto* group = child.node->asMetaGroup();
      const auto* leaf = child.node->asMetaLeaf();

      if (group)
      {
        if (group->isObject())
        {
          completion.configureAsObject();
        }
        else
        {
          completion.configureAsGroup();
        }
      }
      else
      {
        if (leaf->getMethod())
        {
          completion.configureAsMethod();
        }
        else
        {
          completion.configureAsFakeMethod();
        }
      }

      list.push_back(std::move(completion));
    }
  }
}

void completeSources(SourcesFetcher& sourcesFetcher, CompletionList& list, std::string_view currentArgValue)
{
  auto addresses = sourcesFetcher();
  auto root = std::make_unique<MetaGroup>();

  for (const auto& elem: addresses)
  {
    auto addressComponents = impl::split(elem);

    if (addressComponents.size() == 2U)
    {
      auto* sessionGroup = getOrCreateParentGroup(root.get(), elem);
      auto leaf = std::make_unique<MetaLeaf>();

      leaf->setCompletePath(elem);
      leaf->setInstance(nullptr);
      leaf->setMethod(nullptr);
      sessionGroup->addChild(addressComponents.back(), std::move(leaf));
    }
    else if (addressComponents.size() == 1U)
    {
      auto leaf = std::make_unique<MetaLeaf>();

      leaf->setCompletePath(elem);
      leaf->setInstance(nullptr);
      leaf->setMethod(nullptr);
      root->addChild(elem, std::move(leaf));
    }
    else
    {
      // no code needed
    }
  }

  completeFromTree(root.get(), currentArgValue, list);
}

void completeInfo(kernel::RunApi& api,
                  const std::list<Object*>& instances,
                  CompletionList& list,
                  std::string_view currentArgValue)
{
  auto root = std::make_unique<MetaGroup>();

  // add instances
  for (const auto* instance: instances)
  {
    auto completePath = computeInstanceName(instance);
    auto* group = getOrCreateParentGroup(root.get(), completePath);
    auto leaf = std::make_unique<MetaLeaf>();
    leaf->setInstance(nullptr);
    leaf->setMethod(nullptr);
    leaf->setCompletePath(completePath);
    group->addChild(::sen::impl::split(instance->getName(), '.').back(), std::move(leaf));
  }

  // add types
  const auto& types = api.getTypes().getAll();
  for (const auto& [name, type]: types)
  {
    const std::string completePath(type->getQualifiedName());
    auto* group = getOrCreateParentGroup(root.get(), completePath);
    auto leaf = std::make_unique<MetaLeaf>();
    leaf->setInstance(nullptr);
    leaf->setMethod(nullptr);
    leaf->setCompletePath(completePath);
    group->addChild(std::string(type->getName()), std::move(leaf));
  }

  completeFromTree(root.get(), currentArgValue, list);
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Completion
//--------------------------------------------------------------------------------------------------------------

Completion::Completion(): style_(completionStyle) {}

void Completion::configureAsObject() { style_ = objectCompletionStyle; }

void Completion::configureAsMethod() { style_ = methodCompletionStyle; }

void Completion::configureAsGroup() { style_ = groupCompletionStyle; }

void Completion::configureAsFakeMethod() { style_ = fakeMethodCompletionStyle; }

void Completion::set(std::string str) noexcept { str_ = std::move(str); }

const std::string& Completion::get() const noexcept { return str_; }

Style Completion::getStyle() const noexcept { return style_; }

//--------------------------------------------------------------------------------------------------------------
// Completer
//--------------------------------------------------------------------------------------------------------------

Completer::Completer(const ObjectList<Object>* objects) noexcept: objects_(objects) {}

void Completer::getCompletions(const std::string& buffer,
                               kernel::RunApi& api,
                               SourcesFetcher& sourcesFetcher,
                               CompletionList& list,
                               std::string& commonPrefix) const
{
  list.clear();

  if (buffer.empty())
  {
    const auto instances = objects_->getObjects();
    auto root = createRoot(instances);
    completeOnEmptyCommand(root.get(), list);
  }
  else
  {
    std::size_t cmdLastPos = buffer.find_first_of(' ');
    const std::string baseCmd = (cmdLastPos == std::string::npos ? buffer : buffer.substr(0, cmdLastPos));

    if (baseCmd == "open" || baseCmd == "close")
    {
      completeSources(sourcesFetcher, list, std::string_view {buffer}.substr(cmdLastPos + 1U));
    }
    else if (baseCmd == "info")
    {
      completeInfo(api, objects_->getObjects(), list, std::string_view {buffer}.substr(cmdLastPos + 1U));
    }
    else if (cmdLastPos == std::string::npos)
    {
      const auto instances = objects_->getObjects();
      auto root = createRoot(instances);
      completeFromTree(root.get(), baseCmd, list);
    }
    else
    {
      // no code needed
    }
  }

  removeDuplicates(list);
  computeCommonPrefix(buffer, list, commonPrefix);
}

}  // namespace sen::components::shell
