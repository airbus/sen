// === mkdocs_generator.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/mkdocs.h"

// lib
#include "common/type_storage.h"
#include "common/util.h"
#include "mkdocs/classdoc_tree.h"
#include "mkdocs/mkdocs_templates.h"

// templates (baked symbols)
#include "mkdocs_templates/file_doc.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/u8string_util.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/variant_type.h"

// inja
#include <inja/environment.hpp>
#include <inja/json.hpp>
#include <inja/template.hpp>

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::gen
{

using sen::std_util::fromU8string;

namespace
{

[[nodiscard]] std::string toAnchorStr(std::string_view name, std::string_view link, std::string_view description = "")
{
  std::string result = "[";
  result.append(name);
  result.append("](#");

  for (auto character: link)
  {
    if (character != '.')
    {
      result.append(1, static_cast<char>(std::tolower(character)));
    }
  }

  if (!description.empty())
  {
    result.append(" \"");
    result.append(description);
    result.append("\"");
  }

  result.append(")");

  return result;
}

[[nodiscard]] std::string toAnchor(const sen::Type* meta, bool useQualType = true, bool includeDescription = true)
{
  const auto description = includeDescription ? meta->getDescription() : "";

  if (meta->isOptionalType())
  {
    meta = meta->asOptionalType()->getType().type();
  }

  if (const auto* custom = meta->asCustomType(); meta->isCustomType())
  {
    if (useQualType)
    {
      return toAnchorStr(custom->getQualifiedName(), custom->getQualifiedName(), description);
    }

    return toAnchorStr(meta->getName(), custom->getQualifiedName(), description);
  }
  return toAnchorStr(meta->getName(), meta->getName(), description);
}

[[nodiscard]] std::string toAnchor(const inja::json& data)
{
  return toAnchorStr(data.get<std::string>(), data.get<std::string>());
}

class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static void generate(const sen::CustomType& type,
                       inja::Environment& env,
                       detail::TypeStorage& typeStorage,
                       const std::string& packageName,
                       const detail::MkDocsTemplates& templates,
                       detail::TemplateVisitorResult& result)
  {
    TemplateVisitor visitor(env, typeStorage, packageName, templates, result);
    type.accept(visitor);
  }

protected:
  void apply(const sen::Type& type) override
  {
    std::string err;
    err.append("unsupported type '");
    err.append(type.getName());
    err.append("'");
    sen::throwRuntimeError(err);
  }

  void apply(const sen::StructType& type) override
  {
    compute(type, templates_.structTemplate, result_.structuresDoc);
    result_.groups.structTypes.push_back(&type);
  }

  void apply(const sen::EnumType& type) override
  {
    compute(type, templates_.enumTemplate, result_.enumerationsDoc);
    result_.groups.enumTypes.push_back(&type);
  }

  void apply(const sen::VariantType& type) override
  {
    compute(type, templates_.variantTemplate, result_.variantsDoc);
    result_.groups.variantTypes.push_back(&type);
  }

  void apply(const sen::SequenceType& type) override
  {
    compute(type, templates_.sequenceTemplate, result_.sequencesDoc);
    result_.groups.sequenceTypes.push_back(&type);
  }

  void apply(const sen::AliasType& type) override
  {
    compute(type, templates_.aliasTemplate, result_.aliasesDoc);
    result_.groups.aliasTypes.push_back(&type);
  }

  void apply(const sen::OptionalType& type) override
  {
    compute(type, templates_.optionalTemplate, result_.optionalsDoc);
    result_.groups.optionalTypes.push_back(&type);
  }

  void apply(const sen::QuantityType& type) override
  {
    compute(type, templates_.quantityTemplate, result_.quantitiesDoc);
    result_.groups.quantityTypes.push_back(&type);
  }

  void apply(const sen::ClassType& type) override
  {
    auto typeInfo = typeStorage_.getOrCreate(type);
    typeInfo["package"] = packageName_;
    typeInfo["level"] = 1;

    result_.classesDoc.append(env_.render(templates_.classTemplate, typeInfo));
    result_.groups.classTypes.push_back(&type);
  }

private:
  TemplateVisitor(inja::Environment& env,
                  detail::TypeStorage& typeStorage,
                  std::string packageName,
                  const detail::MkDocsTemplates& templates,
                  detail::TemplateVisitorResult& result) noexcept
    : env_(env), typeStorage_(typeStorage), packageName_(std::move(packageName)), templates_(templates), result_(result)
  {
  }

  template <typename T>
  inline void compute(const T& type, const inja::Template& templateObject, std::string& target)
  {
    auto typeInfo = typeStorage_.getOrCreate(type);
    typeInfo["package"] = packageName_;
    target.append(env_.render(templateObject, typeInfo));
  }

private:
  inja::Environment& env_;
  detail::TypeStorage& typeStorage_;
  std::string packageName_;
  const detail::MkDocsTemplates& templates_;
  detail::TemplateVisitorResult& result_;
};

void getClassPath(const sen::ClassType* current, std::vector<const sen::ClassType*>& path)
{
  if (current->getParents().empty())
  {
    path.push_back(current);
  }
  else
  {
    getClassPath(current->getParents().front().type(), path);
    path.push_back(current);
  }
}

[[nodiscard]] std::string computePackageName(const sen::lang::TypeSet& set)
{
  std::string result;
  for (std::size_t i = 0U; i < set.package.size(); ++i)
  {
    result.append(set.package[i]);
    if (i != set.package.size() - 1U)
    {
      result.append(".");
    }
  }

  return result;
}

[[nodiscard]] uint32_t getLevel(const sen::ClassType* meta)
{
  if (meta->getParents().empty())
  {
    return 1;
  }

  return 1 + getLevel(meta->getParents().front().type());
}

[[nodiscard]] std::string makeClassNodeData(detail::Node* currentNode,
                                            inja::Environment& env,
                                            const inja::Template& tmp)
{
  std::string result;

  if (auto meta = currentNode->getMeta(); meta != nullptr)
  {
    auto typeInfo = currentNode->getStorage()->getOrCreate(*meta);
    typeInfo["level"] = getLevel(meta);

    result = env.render(tmp, typeInfo);
  }

  for (auto& child: currentNode->getChildren())
  {
    result.append(makeClassNodeData(&child, env, tmp));
  }

  return result;
}

void print(detail::Node* currentNode,
           inja::Environment& env,
           std::string indent,
           bool last,
           bool root,
           std::string& result)
{
  result.append(indent);

  auto& children = currentNode->getChildren();
  const auto* meta = currentNode->getMeta();

  if (root)
  {
    result.append(fromU8string(u8"┬"));
    result.append(meta ? toAnchor(meta) : "");
    result.append("\n");
  }
  else
  {
    if (last)
    {
      result.append(fromU8string(u8"└"));
      indent.append("  ");
    }
    else
    {
      result.append(fromU8string(u8"├"));
      indent.append(fromU8string(u8"│ "));
    }

    result.append(fromU8string(u8"─"));

    if (!children.empty())
    {
      result.append(fromU8string(u8"┬"));
    }
    else
    {
      result.append(fromU8string(u8"─"));
    }

    result.append(fromU8string(u8"─"));
    result.append(toAnchor(meta));
    result.append("\n");
  }

  for (size_t i = 0; i < children.size(); ++i)
  {
    print(&children[i], env, indent, i == children.size() - 1, false, result);
  }
}

[[nodiscard]] std::string makeClassHierarchy(detail::Node* currentNode, inja::Environment& env)
{
  std::string indent(8, ' ');
  std::string result;

  if (currentNode->getMeta() == nullptr && currentNode->getChildren().size() == 1 &&
      currentNode->getChildren().front().getMeta()->getName() == "ObjectRoot")
  {
    print(&currentNode->getChildren().front(), env, indent, false, true, result);
  }
  else
  {
    print(currentNode, env, indent, false, true, result);
  }
  return result;
}

struct TableCell
{
  std::string content;
  std::size_t effectiveSize;
};

using Row = std::vector<TableCell>;
using Table = std::vector<Row>;

void writeTable(const Table& table, unsigned indentation, std::string& result)
{
  if (table.empty())
  {
    return;
  }

  const auto& header = table.front();

  // calc the max widths
  std::vector<size_t> maxWidths(header.size(), 0);

  for (const auto& row: table)
  {
    for (size_t j = 0; j < row.size(); ++j)
    {
      maxWidths[j] = std::max(maxWidths[j], row[j].effectiveSize);
    }
  }

  result.append(std::string(indentation, ' '));

  for (std::size_t i = 0; i < header.size(); ++i)
  {
    result.append("| ");
  }
  result.append("|\n");

  result.append(std::string(indentation, ' '));
  for (std::size_t i = 0; i < header.size(); ++i)
  {
    result.append("| :-- ");
  }
  result.append("|\n");

  // write rows
  for (const auto& row: table)
  {
    result.append(std::string(indentation, ' '));

    for (size_t j = 0; j < row.size(); ++j)
    {
      if (j == row.size() - 1)  // last column
      {
        const auto& lastColumn = row[j];

        result.append("| ");
        result.append(lastColumn.content);
      }
      else
      {
        result.append("| ");
        result.append(row[j].content);

        auto width = maxWidths[j] - row[j].effectiveSize;
        if (width > 0)
        {
          result.append(std::string(width, ' '));
        }
      }
    }
    result.append("| ");
    result.append("\n");
  }
}

std::string printTable(const std::vector<const sen::Type*>& elements)
{
  // create the table
  const size_t cols = 4;
  Table table;

  // fill the table
  Row row;
  for (const auto& element: elements)
  {
    TableCell cell;
    cell.content = toAnchor(element, false, true);
    cell.effectiveSize = element->asCustomType()->getQualifiedName().size();

    row.push_back(cell);

    if (row.size() == cols)
    {
      table.push_back(row);
      row.clear();
    }
  }

  // add the last row
  if (!row.empty())
  {
    table.push_back(row);
  }

  // draw table
  std::string result;
  writeTable(table, 8, result);
  return result;
}

template <typename T>
void append(const std::vector<T>& src, std::vector<T>& target)
{
  target.insert(target.end(), src.begin(), src.end());
}

[[nodiscard]] std::string generateFile(inja::Environment& env,
                                       std::vector<std::shared_ptr<detail::TypeStorage>>& storage,
                                       const detail::MkDocsTemplates& templates,
                                       const inja::Template& fileTemplate,
                                       const std::string& title)
{
  std::vector<inja::json> clustersDataList;

  detail::Node rootNode(nullptr, nullptr, nullptr);

  std::string enumerationsDoc;
  std::string classesDoc;
  std::string sequencesDoc;
  std::string variantsDoc;
  std::string quantitiesDoc;
  std::string structuresDoc;
  std::string aliasesDoc;
  std::string optionalsDoc;

  std::map<std::string, detail::TypeGroups> typesPerPackage;

  for (auto& storageElem: storage)
  {
    auto set = storageElem->getTypeSet();
    auto packageName = computePackageName(set);

    detail::TemplateVisitorResult result;

    for (const auto& type: set.types)
    {
      TemplateVisitor::generate(*type, env, *storageElem, packageName, templates, result);

      if (type->isClassType())
      {
        std::vector<const sen::ClassType*> path;
        getClassPath(type->asClassType(), path);
        std::ignore = rootNode.getOrCreateChild(path, storageElem.get());
      }
    }

    {
      auto& packageData = typesPerPackage[packageName];

      append(result.groups.enumTypes, packageData.enumTypes);
      append(result.groups.sequenceTypes, packageData.sequenceTypes);
      append(result.groups.variantTypes, packageData.variantTypes);
      append(result.groups.quantityTypes, packageData.quantityTypes);
      append(result.groups.structTypes, packageData.structTypes);
      append(result.groups.aliasTypes, packageData.aliasTypes);
      append(result.groups.optionalTypes, packageData.optionalTypes);
    }

    enumerationsDoc.append(result.enumerationsDoc);
    classesDoc.append(result.classesDoc);
    sequencesDoc.append(result.sequencesDoc);
    variantsDoc.append(result.variantsDoc);
    quantitiesDoc.append(result.quantitiesDoc);
    structuresDoc.append(result.structuresDoc);
    aliasesDoc.append(result.aliasesDoc);
    optionalsDoc.append(result.optionalsDoc);
  }

  // transform the map into a vector
  std::vector<inja::json> packages;
  for (const auto& [name, types]: typesPerPackage)
  {
    inja::json packageData;
    packageData["name"] = name;
    packageData["structTable"] = printTable(types.structTypes);
    packageData["variantTable"] = printTable(types.variantTypes);
    packageData["sequenceTable"] = printTable(types.sequenceTypes);
    packageData["quantityTable"] = printTable(types.quantityTypes);
    packageData["enumTable"] = printTable(types.enumTypes);
    packageData["aliasTable"] = printTable(types.aliasTypes);
    packageData["optionalTable"] = printTable(types.optionalTypes);

    packages.push_back(packageData);
  }

  inja::json fileData;

  // documentation sections
  fileData["enumerations"] = enumerationsDoc;
  fileData["classes"] = classesDoc;
  fileData["sequences"] = sequencesDoc;
  fileData["variants"] = variantsDoc;
  fileData["quantities"] = quantitiesDoc;
  fileData["structures"] = structuresDoc;
  fileData["aliases"] = aliasesDoc;
  fileData["optionals"] = optionalsDoc;
  fileData["packages"] = packages;
  fileData["classHierarchy"] = makeClassHierarchy(&rootNode, env);
  fileData["classesRoot"] = makeClassNodeData(&rootNode, env, templates.classTemplate);

  // other data
  fileData["title"] = title;

  return env.render(fileTemplate, fileData);
}

}  // namespace

class MkDocsGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl()
  {
    detail::configureEnv(env_);
    env_.set_line_statement("***");
    env_.add_callback("toAnchor", 1, [](auto& args) { return toAnchor(*args.front()); });
    templates_ = detail::makeMkDocsTemplates(env_);
    fileTemplate_ = env_.parse(sen::decompressSymbolToString(file_doc, file_docSize));
  }
  ~Impl() = default;

public:
  std::string generate(const sen::lang::TypeSetContext& typeSets, const std::string& title)
  {
    std::vector<std::shared_ptr<detail::TypeStorage>> storage;
    storage.reserve(typeSets.size());
    for (const auto& elem: typeSets)
    {
      storage.emplace_back(std::make_shared<detail::TypeStorage>(elem));
    }

    return generateFile(env_, storage, templates_, fileTemplate_, title);
  }

private:
  inja::Environment env_;
  detail::MkDocsTemplates templates_;
  inja::Template fileTemplate_;
};

//--------------------------------------------------------------------------------------------------------------
// MkDocsGenerator
//--------------------------------------------------------------------------------------------------------------

MkDocsGenerator::MkDocsGenerator(): pimpl_(std::make_unique<Impl>()) {}

MkDocsGenerator::~MkDocsGenerator() = default;

std::string MkDocsGenerator::generate(const sen::lang::TypeSetContext& typeSets, const std::string& title)
{
  return pimpl_->generate(typeSets, title);
}

}  // namespace sen::gen
