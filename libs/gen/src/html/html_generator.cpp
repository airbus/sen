// === html_generator.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/html.h"

// lib
#include "html/html_app.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/base/version.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/variant_type.h"

// inja
#include <inja/json.hpp>

// std
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::gen
{

namespace
{

// What a kind is called on screen. Both forms, because English does not pluralise by rule.
struct KindName
{
  std::string_view id;
  std::string_view one;
  std::string_view many;
};

constexpr std::array<KindName, 10> kindNames {{{"classes", "class", "classes"},
                                               {"structures", "structure", "structures"},
                                               {"enumerations", "enumeration", "enumerations"},
                                               {"variants", "variant", "variants"},
                                               {"sequences", "sequence", "sequences"},
                                               {"aliases", "alias", "aliases"},
                                               {"optionals", "optional", "optionals"},
                                               {"quantities", "quantity", "quantities"},
                                               {"builtins", "built-in type", "built-in types"},
                                               {"types", "type", "types"}}};

// The kind a type is presented as. Groups the tree and colours the filters.
class KindVisitor: protected sen::TypeVisitor
{
public:
  [[nodiscard]] static std::string kindOf(const sen::CustomType& type)
  {
    KindVisitor visitor;
    type.accept(visitor);
    return visitor.kind_;
  }

protected:
  void apply(const sen::Type& /*type*/) override { kind_ = "types"; }
  void apply(const sen::StructType& /*type*/) override { kind_ = "structures"; }
  void apply(const sen::EnumType& /*type*/) override { kind_ = "enumerations"; }
  void apply(const sen::VariantType& /*type*/) override { kind_ = "variants"; }
  void apply(const sen::SequenceType& /*type*/) override { kind_ = "sequences"; }
  void apply(const sen::AliasType& /*type*/) override { kind_ = "aliases"; }
  void apply(const sen::OptionalType& /*type*/) override { kind_ = "optionals"; }
  void apply(const sen::QuantityType& /*type*/) override { kind_ = "quantities"; }
  void apply(const sen::ClassType& /*type*/) override { kind_ = "classes"; }

private:
  std::string kind_;
};

[[nodiscard]] std::string packageOf(const sen::lang::TypeSet& set)
{
  std::string result;
  for (std::size_t i = 0U; i < set.package.size(); ++i)
  {
    result.append(set.package[i]);
    if (i + 1U != set.package.size())
    {
      result.append(".");
    }
  }
  return result;
}

// Description text as one run of prose; the source wraps and indents its sentences.
[[nodiscard]] std::string prose(std::string_view text)
{
  std::string result;
  result.reserve(text.size());
  bool gap = false;
  for (const auto character: text)
  {
    const bool space = character == ' ' || character == '\t' || character == '\n' || character == '\r';
    if (space)
    {
      gap = !result.empty();
      continue;
    }
    if (gap)
    {
      result.push_back(' ');
      gap = false;
    }
    result.push_back(character);
  }
  return result;
}

// The title is the caller's and reaches the page as markup.
[[nodiscard]] std::string escapeHtml(std::string_view text)
{
  std::string result;
  result.reserve(text.size());
  for (const auto character: text)
  {
    switch (character)
    {
      case '&':
        result.append("&amp;");
        break;
      case '<':
        result.append("&lt;");
        break;
      case '>':
        result.append("&gt;");
        break;
      case '"':
        result.append("&quot;");
        break;
      case '\'':
        result.append("&#39;");
        break;
      default:
        result.push_back(character);
        break;
    }
  }
  return result;
}

[[nodiscard]] std::string qualified(const sen::CustomType& type) { return std::string(type.getQualifiedName()); }

// The chain from the root down to this class, first parent at each step.
[[nodiscard]] std::vector<std::string> ancestryOf(const sen::ClassType& type)
{
  std::vector<std::string> chain;
  const auto* current = &type;
  while (!current->getParents().empty())
  {
    current = current->getParents().front().type();
    chain.emplace_back(current->getQualifiedName());
  }
  std::reverse(chain.begin(), chain.end());
  return chain;
}

// Every type the walk names, so anything the model does not declare can be given an entry
// afterwards. Passed down rather than held at file scope, which is not thread-safe.
struct PrimitiveSink
{
  std::map<std::string, const sen::Type*> seen;
};

// The name a reference to this type is written as. A named optional is a type in its own
// right and keeps its name; only an unnamed one stands for what it wraps.
[[nodiscard]] std::string typeNameOf(const sen::Type& type, PrimitiveSink& sink)
{
  if (type.isCustomType())
  {
    auto name = std::string(type.asCustomType()->getQualifiedName());
    // Recorded too: the kernel's own types belong to no type set and would go undefined.
    sink.seen.emplace(name, &type);
    return name;
  }
  if (type.isOptionalType())
  {
    return typeNameOf(*type.asOptionalType()->getType(), sink);
  }
  auto name = std::string(type.getName());
  sink.seen.emplace(name, &type);
  return name;
}

[[nodiscard]] inja::json flagsOf(const sen::Property& prop)
{
  std::vector<std::string> flags;
  const auto category = prop.getCategory();
  const bool writable = category == sen::PropertyCategory::dynamicRW || category == sen::PropertyCategory::staticRW;
  const bool dynamic = category == sen::PropertyCategory::dynamicRO || category == sen::PropertyCategory::dynamicRW;

  flags.emplace_back(writable ? "Writable" : "Read Only");
  flags.emplace_back(dynamic ? "Dynamic" : "Static");
  flags.emplace_back(prop.getTransportMode() == sen::TransportMode::confirmed ? "Confirmed" : "Best Effort");
  return flags;
}

[[nodiscard]] inja::json memberOf(const sen::Property& prop, PrimitiveSink& sink)
{
  inja::json member;
  member["name"] = prop.getName();
  member["type"] = typeNameOf(*prop.getType(), sink);
  member["desc"] = prose(prop.getDescription());
  member["flags"] = flagsOf(prop);
  return member;
}

[[nodiscard]] inja::json argsOf(const sen::Span<const sen::Arg>& args, PrimitiveSink& sink)
{
  std::vector<inja::json> out;
  for (const auto& arg: args)
  {
    inja::json entry;
    entry["name"] = arg.name;
    entry["type"] = typeNameOf(*arg.type, sink);
    entry["desc"] = prose(arg.description);
    out.emplace_back(std::move(entry));
  }
  return out;
}

[[nodiscard]] inja::json callableOf(const sen::Method& method, PrimitiveSink& sink)
{
  inja::json out;
  out["name"] = method.getName();
  out["desc"] = prose(method.getDescription());
  // A void return is the absence of a type, not a type.
  const auto& returned = *method.getReturnType();
  out["returns"] = returned.isVoidType() ? std::string {} : typeNameOf(returned, sink);
  out["args"] = argsOf(method.getArgs(), sink);
  return out;
}

[[nodiscard]] inja::json callableOf(const sen::Event& event, PrimitiveSink& sink)
{
  inja::json out;
  out["name"] = event.getName();
  out["desc"] = prose(event.getDescription());
  out["args"] = argsOf(event.getArgs(), sink);
  return out;
}

// A method or event as a row, so an inherited list can show it beside the properties.
[[nodiscard]] inja::json memberOf(const sen::Method& method, PrimitiveSink& sink)
{
  inja::json member;
  member["name"] = std::string(method.getName()) + "()";
  const auto& returned = *method.getReturnType();
  member["type"] = returned.isVoidType() ? std::string {} : typeNameOf(returned, sink);
  member["desc"] = prose(method.getDescription());
  member["flags"] = inja::json::array();
  return member;
}

[[nodiscard]] inja::json memberOf(const sen::Event& event, PrimitiveSink& /*sink*/)
{
  inja::json member;
  member["name"] = std::string(event.getName()) + "()";
  member["type"] = "";
  member["desc"] = prose(event.getDescription());
  member["flags"] = inja::json::array();
  return member;
}

// What a class inherits rather than declares, grouped by the ancestor declaring it,
// nearest first.
[[nodiscard]] inja::json inheritedOf(const sen::ClassType& type, std::size_t& total, PrimitiveSink& sink)
{
  constexpr auto ownOnly = sen::ClassType::SearchMode::doNotIncludeParents;

  std::vector<inja::json> groups;
  std::vector<std::string> visited;
  std::vector<const sen::ClassType*> frontier;
  total = 0U;

  for (const auto& parent: type.getParents())
  {
    frontier.push_back(&*parent);
  }

  while (!frontier.empty())
  {
    std::vector<const sen::ClassType*> next;
    for (const auto* ancestor: frontier)
    {
      auto name = qualified(*ancestor);
      if (std::find(visited.begin(), visited.end(), name) != visited.end())
      {
        continue;
      }
      visited.push_back(name);

      std::vector<inja::json> members;
      for (const auto& prop: ancestor->getProperties(ownOnly))
      {
        members.push_back(memberOf(*prop, sink));
      }
      for (const auto& method: ancestor->getMethods(ownOnly))
      {
        members.push_back(memberOf(*method, sink));
      }
      for (const auto& event: ancestor->getEvents(ownOnly))
      {
        members.push_back(memberOf(*event, sink));
      }

      if (!members.empty())
      {
        inja::json group;
        group["from"] = name;
        group["count"] = members.size();
        group["members"] = members;
        groups.push_back(group);
        total += members.size();
      }

      for (const auto& grandparent: ancestor->getParents())
      {
        next.push_back(&*grandparent);
      }
    }
    frontier = next;
  }

  return groups;
}

[[nodiscard]] inja::json memberOf(const sen::StructField& field, PrimitiveSink& sink)
{
  inja::json member;
  member["name"] = field.name;
  member["type"] = typeNameOf(*field.type, sink);
  member["desc"] = prose(field.description);
  member["flags"] = inja::json::array();
  return member;
}

// What a struct inherits, grouped by the ancestor declaring it, nearest first. Structs
// inherit singly, so this walks one chain.
[[nodiscard]] inja::json inheritedOf(const sen::StructType& type, std::size_t& total, PrimitiveSink& sink)
{
  std::vector<inja::json> groups;
  total = 0U;

  for (auto parent = type.getParent(); parent; parent = parent.value()->getParent())
  {
    const auto& ancestor = *parent.value();
    std::vector<inja::json> members;
    for (const auto& field: ancestor.getFields())
    {
      members.push_back(memberOf(field, sink));
    }
    if (!members.empty())
    {
      inja::json group;
      group["from"] = std::string(ancestor.getQualifiedName());
      group["count"] = members.size();
      group["members"] = members;
      groups.push_back(group);
      total += members.size();
    }
  }
  return groups;
}

// The chain from the root down to this struct.
[[nodiscard]] std::vector<std::string> ancestryOf(const sen::StructType& type)
{
  std::vector<std::string> chain;
  for (auto parent = type.getParent(); parent; parent = parent.value()->getParent())
  {
    chain.emplace_back(parent.value()->getQualifiedName());
  }
  std::reverse(chain.begin(), chain.end());
  return chain;
}

// The facts that define a type, as fields rather than as prose.
void addFacts(const sen::CustomType& type, inja::json& entry, PrimitiveSink& sink)
{
  inja::json facts = inja::json::object();
  inja::json rows = inja::json::array();
  inja::json header = inja::json::array();

  if (type.isEnumType())
  {
    const auto& enumType = *type.asEnumType();
    facts["representation"] = typeNameOf(enumType.getStorageType(), sink);
    for (const auto& enumerator: enumType.getEnums())
    {
      rows.push_back(inja::json {std::string(enumerator.name), std::to_string(enumerator.key)});
    }
    header = inja::json::array({"Enumerator", "Value"});
  }
  else if (type.isAliasType())
  {
    facts["aliased"] = typeNameOf(*type.asAliasType()->getAliasedType(), sink);
  }
  else if (type.isOptionalType())
  {
    facts["optional"] = typeNameOf(*type.asOptionalType()->getType(), sink);
  }
  else if (type.isQuantityType())
  {
    const auto& quantity = *type.asQuantityType();
    facts["representation"] = typeNameOf(*quantity.getElementType(), sink);
    if (const auto unit = quantity.getUnit(); unit.has_value())
    {
      facts["unit"] = std::string((*unit)->getAbbreviation());
    }
    if (const auto maximum = quantity.getMaxValue(); maximum.has_value())
    {
      facts["max"] = *maximum;
    }
    if (const auto minimum = quantity.getMinValue(); minimum.has_value())
    {
      facts["min"] = *minimum;
    }
  }
  else if (type.isSequenceType())
  {
    const auto& sequence = *type.asSequenceType();
    facts["element"] = typeNameOf(*sequence.getElementType(), sink);
    facts["bounded"] = sequence.isBounded();
    facts["fixedSize"] = sequence.hasFixedSize();
    if (const auto maxSize = sequence.getMaxSize(); maxSize.has_value())
    {
      facts["maxSize"] = *maxSize;
    }
  }
  else if (type.isVariantType())
  {
    for (const auto& field: type.asVariantType()->getFields())
    {
      rows.push_back(inja::json {typeNameOf(*field.type, sink), prose(field.description)});
    }
    header = inja::json::array({"Type", "Description"});
  }

  entry["facts"] = facts;
  entry["table"] = {{"header", header}, {"rows", rows}};
}

[[nodiscard]] inja::json entryFor(const sen::CustomType& type, const std::string& package, PrimitiveSink& sink)
{
  const auto name = qualified(type);

  inja::json entry;
  entry["name"] = name;
  entry["package"] = package;
  entry["kind"] = KindVisitor::kindOf(type);
  entry["desc"] = prose(type.getDescription());
  entry["ancestry"] = inja::json::array();
  entry["groups"] = inja::json::array();
  entry["methods"] = inja::json::array();
  entry["events"] = inja::json::array();

  std::vector<inja::json> members;
  std::size_t inherited = 0U;

  if (type.isClassType())
  {
    constexpr auto ownOnly = sen::ClassType::SearchMode::doNotIncludeParents;
    const auto& classType = *type.asClassType();

    for (const auto& prop: classType.getProperties(ownOnly))
    {
      members.push_back(memberOf(*prop, sink));
    }
    std::vector<inja::json> methods;
    std::vector<inja::json> events;
    for (const auto& method: classType.getMethods(ownOnly))
    {
      methods.push_back(callableOf(*method, sink));
    }
    for (const auto& event: classType.getEvents(ownOnly))
    {
      events.push_back(callableOf(*event, sink));
    }
    entry["methods"] = methods;
    entry["events"] = events;
    entry["ancestry"] = ancestryOf(classType);
    entry["groups"] = inheritedOf(classType, inherited, sink);
  }
  else if (type.isStructType())
  {
    const auto& structType = *type.asStructType();
    for (const auto& field: structType.getFields())
    {
      members.push_back(memberOf(field, sink));
    }
    entry["ancestry"] = ancestryOf(structType);
    entry["groups"] = inheritedOf(structType, inherited, sink);
  }

  addFacts(type, entry, sink);

  // Counted the way the inherited side counts, or the two disagree.
  const auto declared = members.size() + entry["methods"].size() + entry["events"].size();
  entry["members"] = members;
  entry["direct"] = declared;
  entry["inherited"] = inherited;
  entry["total"] = declared + inherited;
  return entry;
}

// An entry for everything the walk named that the model does not declare: the primitives,
// and the kernel's own types, which belong to no type set.
void addPrimitives(inja::json& types, const PrimitiveSink& sink)
{
  for (const auto& [name, type]: sink.seen)
  {
    if (types.contains(name))
    {
      continue;
    }
    inja::json entry;
    entry["name"] = name;
    entry["package"] = "";
    // Built-in whatever their shape. Filing one by its real kind would promise facts that
    // are not gathered for it.
    entry["kind"] = "builtins";
    entry["desc"] = prose(type->getDescription());
    entry["ancestry"] = inja::json::array();
    entry["groups"] = inja::json::array();
    entry["members"] = inja::json::array();
    entry["methods"] = inja::json::array();
    entry["events"] = inja::json::array();
    entry["facts"] = inja::json::object();
    entry["table"] = {{"header", inja::json::array()}, {"rows", inja::json::array()}};
    entry["direct"] = 0;
    entry["inherited"] = 0;
    entry["total"] = 0;
    types[name] = entry;
  }
}

// How many of each package and kind the output holds. Counted from the entries rather than
// while walking, so a name declared twice cannot be counted twice.
[[nodiscard]] inja::json countsOf(const inja::json& types)
{
  std::map<std::string, std::size_t> packages;
  std::map<std::string, std::size_t> kinds;
  for (const auto& [name, entry]: types.items())
  {
    if (const auto package = entry["package"].get<std::string>(); !package.empty())
    {
      packages[package]++;
    }
    kinds[entry["kind"].get<std::string>()]++;
  }
  return {{"types", types.size()}, {"packages", packages}, {"kinds", kinds}};
}

// Which types refer to each one; the model does not expose it. A type can be named from a
// parent, a member, a fact, a variant's alternatives or a callable.
void addUsedBy(inja::json& types)
{
  std::map<std::string, std::vector<std::string>> usedBy;

  const auto note = [&](const inja::json& target, const std::string& from)
  {
    if (!target.is_string())
    {
      return;
    }
    const auto name = target.get<std::string>();
    if (!name.empty() && name != from && types.contains(name))
    {
      usedBy[name].push_back(from);
    }
  };

  for (const auto& [name, entry]: types.items())
  {
    // What a type is built on is a use of it, and the one most worth counting.
    if (!entry["ancestry"].empty())
    {
      note(entry["ancestry"].back(), name);
    }
    for (const auto& member: entry["members"])
    {
      note(member["type"], name);
    }
    for (const auto& key: {"element", "optional", "aliased", "representation"})
    {
      if (entry["facts"].contains(key))
      {
        note(entry["facts"][key], name);
      }
    }
    // Variants only. An enumeration's first column holds enumerator names, which may legally
    // collide with a type name and are not references.
    if (entry["kind"] == "variants")
    {
      for (const auto& row: entry["table"]["rows"])
      {
        if (!row.empty())
        {
          note(row[0], name);
        }
      }
    }
    for (const auto& list: {"methods", "events"})
    {
      for (const auto& callable: entry[list])
      {
        if (callable.contains("returns"))
        {
          note(callable["returns"], name);
        }
        for (const auto& arg: callable["args"])
        {
          note(arg["type"], name);
        }
      }
    }
  }

  for (auto& [name, entry]: types.items())
  {
    auto& list = usedBy[name];
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    entry["usedBy"] = list;
    entry["fanIn"] = list.size();
  }
}

// Characters that are ordinary in JSON and not ordinary where this text ends up. U+2028 and
// U+2029 are JavaScript line terminators before ES2019; `<` ends a script element at the first
// `</script`, whatever the JavaScript around it is doing.
[[nodiscard]] std::string escapeForScriptElement(std::string text)
{
  // the UTF-8 bytes for U+2028 and U+2029
  static constexpr std::string_view lineSeparator {"\xE2\x80\xA8"};
  static constexpr std::string_view paragraphSeparator {"\xE2\x80\xA9"};
  static constexpr std::string_view lessThan {"<"};
  for (const auto& [from, to]: {std::pair {lineSeparator, "\\u2028"},
                                std::pair {paragraphSeparator, "\\u2029"},
                                std::pair {lessThan, "\\u003C"}})
  {
    for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + 6U))
    {
      text.replace(at, from.size(), to);
    }
  }
  return text;
}

[[nodiscard]] std::string senVersion() { return SEN_VERSION_STRING; }

}  // namespace

class HtmlGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl(): app_(detail::makeHtmlApp()) {}
  ~Impl() = default;

  HtmlGenerator::FileContents generate(const sen::lang::TypeSetContext& typeSets, const std::string& title)
  {
    PrimitiveSink sink;

    inja::json types = inja::json::object();

    for (const auto& set: typeSets)
    {
      const auto package = packageOf(set);
      for (const auto& handle: set.types)
      {
        auto entry = entryFor(*handle, package, sink);
        auto name = entry["name"].get<std::string>();
        types[name] = std::move(entry);
      }
    }

    addPrimitives(types, sink);
    addUsedBy(types);

    inja::json meta;
    meta["title"] = title;
    meta["generator"] = "Sen " + senVersion();
    // RFC 3339 in UTC, so it means the same wherever it is opened. The page presents it.
    meta["generated"] =
      sen::TimeStamp {sen::Duration {std::chrono::system_clock::now().time_since_epoch()}}.toUtcStringNs();
    meta["counts"] = countsOf(types);
    inja::json names = inja::json::object();
    for (const auto& kind: kindNames)
    {
      names[std::string {kind.id}] = {{"one", kind.one}, {"many", kind.many}};
    }
    meta["kinds"] = names;

    inja::json model;
    model["meta"] = meta;
    model["types"] = types;

    auto page = app_.shell;
    const std::string token = "{{ title }}";
    const auto safeTitle = escapeHtml(title);
    for (auto at = page.find(token); at != std::string::npos; at = page.find(token, at + safeTitle.size()))
    {
      page.replace(at, token.size(), safeTitle);
    }

    HtmlGenerator::FileContents files;
    files.emplace("index.html", std::move(page));
    files.emplace("app.css", app_.styles);
    files.emplace("app.js", app_.script);
    files.emplace("logo.svg", app_.logo);
    // The kernel does not require a description to be UTF-8; a strict dump would refuse one.
    const auto json = model.dump(-1, ' ', false, inja::json::error_handler_t::replace);
    // Parsed, not evaluated: in a literal a key of "__proto__" sets a prototype.
    files.emplace("model.js",
                  "window.__senModel=JSON.parse(" + escapeForScriptElement(inja::json(json).dump()) + ");\n");
    return files;
  }

private:
  detail::HtmlApp app_;
};

HtmlGenerator::HtmlGenerator(): pimpl_(std::make_unique<Impl>()) {}

HtmlGenerator::~HtmlGenerator() = default;

HtmlGenerator::FileContents HtmlGenerator::generate(const sen::lang::TypeSetContext& typeSets, const std::string& title)
{
  return pimpl_->generate(typeSets, title);
}

}  // namespace sen::gen
