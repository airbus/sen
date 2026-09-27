// === command_engine_inspect.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "command_engine.h"

// component
#include "byte_format.h"
#include "styles.h"
#include "text_table.h"
#include "type_peel.h"
#include "unicode.h"
#include "util.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/base/version.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/variant_type.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Build a compact "name(arg1: Type, arg2: Type)" signature string.
std::string formatArgSignature(std::string_view name, Span<const Arg> args)
{
  std::string sig(name);
  sig += '(';
  for (std::size_t i = 0; i < args.size(); ++i)
  {
    if (i > 0)
    {
      sig += ", ";
    }
    sig += std::string(args[i].name) + ": " + std::string(args[i].type->getName());
  }
  sig += ')';
  return sig;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Inspect commands
//--------------------------------------------------------------------------------------------------------------

void CommandEngine::cmdInspect(std::string_view args)
{
  if (args.empty())
  {
    reportError("Usage", "inspect <object | type>");
    return;
  }

  auto target = completer_.findObject(args);
  if (!target)
  {
    auto typeHandle = api_.getTypes().get(std::string(args));
    if (typeHandle.has_value())
    {
      inspectType(*typeHandle.value());
      return;
    }
    reportError("Not Found", "'" + std::string(args) + "' is not a known object or type.");
    return;
  }

  const auto* classType = target->getClass().type();

  // Emit header + description as top-level elements so paragraph wraps at the pane's width.
  {
    ftxui::Elements classRow = {ftxui::text(std::string(classType->getQualifiedName())) | styles::typeName()};
    const auto& parents = classType->getParents();
    if (!parents.empty())
    {
      classRow.push_back(ftxui::text(" : ") | styles::mutedText());
      for (std::size_t i = 0; i < parents.size(); ++i)
      {
        if (i > 0)
        {
          classRow.push_back(ftxui::text(", ") | styles::mutedText());
        }
        classRow.push_back(ftxui::text(std::string(parents[i]->getQualifiedName())) | styles::typeName() |
                           styles::mutedText());
      }
    }
    app_.appendElement(ftxui::hbox(std::move(classRow)));
  }

  auto classDesc = classType->getDescription();
  if (!classDesc.empty())
  {
    app_.appendElement(ftxui::paragraph(std::string(classDesc)) | styles::mutedText());
  }

  ftxui::Elements sections;

  auto properties = classType->getProperties(ClassType::SearchMode::includeParents);
  if (!properties.empty())
  {
    sections.push_back(ftxui::text(""));
    sections.push_back(ftxui::text("Properties") | ftxui::bold);

    for (std::size_t i = 0; i < properties.size(); ++i)
    {
      const auto& prop = *properties[i];
      const bool isLast = (i + 1 == properties.size());
      const std::string connector = std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";

      std::string annotation;
      auto category = prop.getCategory();
      if (category == PropertyCategory::dynamicRW)
      {
        annotation = " [writable]";
      }
      else if (category == PropertyCategory::staticRO)
      {
        annotation = " [static]";
      }

      auto typeName = std::string(prop.getType()->getName());

      ftxui::Elements row = {ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                             ftxui::text(std::string(prop.getName())) | ftxui::bold,
                             ftxui::text(" : ") | styles::mutedText(),
                             ftxui::text(typeName) | styles::typeName(),
                             ftxui::text(annotation) | styles::mutedText()};
      auto desc = prop.getDescription();
      if (!desc.empty())
      {
        // flex_shrink: the description gives up its space first, so a narrow terminal clips it
        // rather than the member name and type.
        row.push_back(ftxui::text("  " + std::string(desc)) | styles::mutedText() | ftxui::flex_shrink);
      }
      sections.push_back(ftxui::hbox(std::move(row)));
    }
  }

  auto methods = classType->getMethods(ClassType::SearchMode::includeParents);
  if (!methods.empty())
  {
    sections.push_back(ftxui::text(""));
    sections.push_back(ftxui::text("Methods") | ftxui::bold);

    for (std::size_t i = 0; i < methods.size(); ++i)
    {
      const auto& method = *methods[i];
      const bool isLast = (i + 1 == methods.size());
      const std::string connector = std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";

      auto sig = formatArgSignature(method.getName(), method.getArgs());

      auto retName = std::string(method.getReturnType()->getName());
      bool isVoid = isVoidTypeName(retName);

      ftxui::Elements row = {ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                             ftxui::text(sig) | ftxui::bold};
      if (!isVoid)
      {
        row.push_back(ftxui::text(std::string(" ") + unicode::arrowRight + " ") | styles::mutedText());
        row.push_back(ftxui::text(retName) | styles::typeName());
      }
      auto desc = method.getDescription();
      if (!desc.empty())
      {
        row.push_back(ftxui::text("  " + std::string(desc)) | styles::mutedText() | ftxui::flex_shrink);
      }
      sections.push_back(ftxui::hbox(std::move(row)));
    }
  }

  auto events = classType->getEvents(ClassType::SearchMode::includeParents);
  if (!events.empty())
  {
    sections.push_back(ftxui::text(""));
    sections.push_back(ftxui::text("Events") | ftxui::bold);

    for (std::size_t i = 0; i < events.size(); ++i)
    {
      const auto& ev = *events[i];
      const bool isLast = (i + 1 == events.size());
      const std::string connector = std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";

      auto sig = formatArgSignature(ev.getName(), ev.getArgs());

      ftxui::Elements row = {ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                             ftxui::text(sig) | ftxui::bold};
      auto desc = ev.getDescription();
      if (!desc.empty())
      {
        row.push_back(ftxui::text("  " + std::string(desc)) | styles::mutedText() | ftxui::flex_shrink);
      }
      sections.push_back(ftxui::hbox(std::move(row)));
    }
  }

  app_.appendElement(ftxui::vbox(std::move(sections)));
}

namespace
{

/// The tree connector for a row: a corner for the last child, a tee for the rest.
[[nodiscard]] std::string rowConnector(bool isLast)
{
  return std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";
}

/// A titled block of rows, or nothing when there are no rows.
[[nodiscard]] ftxui::Elements titledSection(std::string_view title, ftxui::Elements rows)
{
  if (rows.empty())
  {
    return {};
  }
  ftxui::Elements out {ftxui::text(""), ftxui::text(std::string(title)) | ftxui::bold};
  for (auto& row: rows)
  {
    out.push_back(std::move(row));
  }
  return out;
}

void append(ftxui::Elements& into, ftxui::Elements from)
{
  for (auto& e: from)
  {
    into.push_back(std::move(e));
  }
}

[[nodiscard]] ftxui::Elements structFields(const StructType& structType)
{
  ftxui::Elements rows;
  auto fields = structType.getAllFields();
  for (std::size_t i = 0; i < fields.size(); ++i)
  {
    const auto& field = fields[i];
    ftxui::Elements row = {ftxui::text(rowConnector(i + 1 == fields.size())) | ftxui::color(styles::treeConnector()),
                           ftxui::text(field.name) | ftxui::bold,
                           ftxui::text(" : ") | styles::mutedText(),
                           ftxui::text(std::string(field.type->getName())) | styles::typeName()};
    if (!field.description.empty())
    {
      row.push_back(ftxui::text("  " + field.description) | styles::mutedText() | ftxui::flex_shrink);
    }
    rows.push_back(ftxui::hbox(std::move(row)));
  }
  return titledSection("Fields", std::move(rows));
}

[[nodiscard]] ftxui::Elements enumValues(const EnumType& enumType)
{
  ftxui::Elements rows;
  auto enums = enumType.getEnums();
  for (std::size_t i = 0; i < enums.size(); ++i)
  {
    ftxui::Elements row = {ftxui::text(rowConnector(i + 1 == enums.size())) | ftxui::color(styles::treeConnector()),
                           ftxui::text(enums[i].name) | ftxui::bold,
                           ftxui::text(" = ") | styles::mutedText(),
                           ftxui::text(std::to_string(enums[i].key)) | styles::typeName()};
    if (!enums[i].description.empty())
    {
      row.push_back(ftxui::text("  " + enums[i].description) | styles::mutedText() | ftxui::flex_shrink);
    }
    rows.push_back(ftxui::hbox(std::move(row)));
  }
  return titledSection("Values", std::move(rows));
}

[[nodiscard]] ftxui::Elements classProperties(const ClassType& classType)
{
  ftxui::Elements rows;
  auto properties = classType.getProperties(ClassType::SearchMode::includeParents);
  for (std::size_t i = 0; i < properties.size(); ++i)
  {
    const auto& prop = *properties[i];
    std::string annotation;
    if (prop.getCategory() == PropertyCategory::dynamicRW)
    {
      annotation = " [writable]";
    }
    else if (prop.getCategory() == PropertyCategory::staticRO)
    {
      annotation = " [static]";
    }
    rows.push_back(
      ftxui::hbox({ftxui::text(rowConnector(i + 1 == properties.size())) | ftxui::color(styles::treeConnector()),
                   ftxui::text(std::string(prop.getName())) | ftxui::bold,
                   ftxui::text(" : ") | styles::mutedText(),
                   ftxui::text(std::string(prop.getType()->getName())) | styles::typeName(),
                   ftxui::text(annotation) | styles::mutedText()}));
  }
  return titledSection("Properties", std::move(rows));
}

[[nodiscard]] ftxui::Elements classMethods(const ClassType& classType)
{
  ftxui::Elements rows;
  auto methods = classType.getMethods(ClassType::SearchMode::includeParents);
  for (std::size_t i = 0; i < methods.size(); ++i)
  {
    const auto& method = *methods[i];
    auto retName = std::string(method.getReturnType()->getName());
    ftxui::Elements row = {ftxui::text(rowConnector(i + 1 == methods.size())) | ftxui::color(styles::treeConnector()),
                           ftxui::text(formatArgSignature(method.getName(), method.getArgs())) | ftxui::bold};
    if (!isVoidTypeName(retName))
    {
      row.push_back(ftxui::text(std::string(" ") + unicode::arrowRight + " ") | styles::mutedText());
      row.push_back(ftxui::text(retName) | styles::typeName());
    }
    rows.push_back(ftxui::hbox(std::move(row)));
  }
  return titledSection("Methods", std::move(rows));
}

[[nodiscard]] ftxui::Elements classEvents(const ClassType& classType)
{
  ftxui::Elements rows;
  auto events = classType.getEvents(ClassType::SearchMode::includeParents);
  for (std::size_t i = 0; i < events.size(); ++i)
  {
    const auto& ev = *events[i];
    rows.push_back(
      ftxui::hbox({ftxui::text(rowConnector(i + 1 == events.size())) | ftxui::color(styles::treeConnector()),
                   ftxui::text(formatArgSignature(ev.getName(), ev.getArgs())) | ftxui::bold}));
  }
  return titledSection("Events", std::move(rows));
}

[[nodiscard]] ftxui::Elements sequenceDetails(const SequenceType& seqType)
{
  ftxui::Elements out {
    ftxui::text(""),
    ftxui::hbox({ftxui::text("Element type: ") | styles::mutedText(),
                 ftxui::text(std::string(seqType.getElementType()->getName())) | styles::typeName()})};
  if (auto max = seqType.getMaxSize(); max.has_value())
  {
    out.push_back(ftxui::hbox({ftxui::text("Max size: ") | styles::mutedText(), ftxui::text(std::to_string(*max))}));
  }
  if (seqType.hasFixedSize())
  {
    out.push_back(ftxui::text("Fixed size (array)") | styles::mutedText());
  }
  return out;
}

[[nodiscard]] ftxui::Elements variantAlternatives(const VariantType& variantType)
{
  ftxui::Elements rows;
  auto fields = variantType.getFields();
  for (std::size_t i = 0; i < fields.size(); ++i)
  {
    rows.push_back(
      ftxui::hbox({ftxui::text(rowConnector(i + 1 == fields.size())) | ftxui::color(styles::treeConnector()),
                   ftxui::text(std::string(fields[i].type->getName())) | styles::typeName()}));
  }
  return titledSection("Alternatives", std::move(rows));
}

[[nodiscard]] ftxui::Elements quantityDetails(const QuantityType& quantityType)
{
  ftxui::Elements out {
    ftxui::text(""),
    ftxui::hbox({ftxui::text("Storage: ") | styles::mutedText(),
                 ftxui::text(std::string(quantityType.getElementType()->getName())) | styles::typeName()})};
  if (auto unit = quantityType.getUnit(); unit.has_value() && *unit != nullptr)
  {
    out.push_back(ftxui::hbox(
      {ftxui::text("Unit: ") | styles::mutedText(),
       ftxui::text(std::string((*unit)->getName()) + " (" + std::string((*unit)->getAbbreviation()) + ")")}));
  }
  if (auto min = quantityType.getMinValue(); min.has_value())
  {
    out.push_back(ftxui::hbox({ftxui::text("Min: ") | styles::mutedText(), ftxui::text(std::to_string(*min))}));
  }
  if (auto max = quantityType.getMaxValue(); max.has_value())
  {
    out.push_back(ftxui::hbox({ftxui::text("Max: ") | styles::mutedText(), ftxui::text(std::to_string(*max))}));
  }
  return out;
}

}  // namespace

void CommandEngine::inspectType(const Type& type)
{
  auto* customType = type.asCustomType();
  auto typeName = customType != nullptr ? std::string(customType->getQualifiedName()) : std::string(type.getName());
  app_.appendElement(ftxui::text(typeName) | styles::typeName());

  auto desc = type.getDescription();
  if (!desc.empty())
  {
    app_.appendElement(ftxui::paragraph(std::string(desc)) | styles::mutedText());
  }

  ftxui::Elements sections;
  if (auto* structType = type.asStructType(); structType != nullptr)
  {
    append(sections, structFields(*structType));
  }
  else if (auto* enumType = type.asEnumType(); enumType != nullptr)
  {
    append(sections, enumValues(*enumType));
  }
  else if (auto* classType = type.asClassType(); classType != nullptr)
  {
    append(sections, classProperties(*classType));
    append(sections, classMethods(*classType));
    append(sections, classEvents(*classType));
  }
  else if (auto* seqType = type.asSequenceType(); seqType != nullptr)
  {
    append(sections, sequenceDetails(*seqType));
  }
  else if (auto* variantType = type.asVariantType(); variantType != nullptr)
  {
    append(sections, variantAlternatives(*variantType));
  }
  else if (auto* quantityType = type.asQuantityType(); quantityType != nullptr)
  {
    append(sections, quantityDetails(*quantityType));
  }

  if (!sections.empty())
  {
    app_.appendElement(ftxui::vbox(std::move(sections)));
  }
}

void CommandEngine::cmdTypes(std::string_view args)
{
  auto allTypes = api_.getTypes().getAll();

  std::string filter = std::string(args);

  ftxui::Elements rows;
  rows.push_back(ftxui::text("Registered types (" + std::to_string(allTypes.size()) + ")") | ftxui::bold);

  std::vector<std::string> names;
  names.reserve(allTypes.size());
  for (const auto& [name, type]: allTypes)
  {
    if (filter.empty() || name.find(filter) != std::string::npos)
    {
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end());

  if (names.empty())
  {
    rows.push_back(ftxui::text("  (no matching types)") | styles::mutedText());
    app_.appendElement(ftxui::vbox(std::move(rows)));
    return;
  }

  std::vector<text_table::Row> tableData;
  tableData.reserve(names.size());
  for (const auto& name: names)
  {
    tableData.push_back({
      {name, ftxui::bold},
      {std::string(typeKindName(*allTypes.at(name).type())), styles::mutedText()},
    });
  }

  rows.push_back(ftxui::hbox({ftxui::text("  "), text_table::render(std::move(tableData))}));

  app_.appendElement(ftxui::vbox(std::move(rows)));
}

void CommandEngine::cmdUnits(std::string_view args)
{
  const auto& registry = UnitRegistry::get();

  std::string filter = std::string(args);

  ftxui::Elements rows;
  bool anyOutput = false;

  for (auto cat: allUnitCategories)
  {
    auto catName = std::string(Unit::getCategoryString(cat));
    if (!filter.empty() && catName.find(filter) == std::string::npos)
    {
      continue;
    }
    auto units = registry.getUnitsByCategory(cat);
    if (units.empty())
    {
      continue;
    }

    if (anyOutput)
    {
      rows.push_back(ftxui::text(""));
    }
    rows.push_back(ftxui::text("  " + catName) | ftxui::bold);

    std::vector<text_table::Row> tableData;
    for (const auto* unit: units)
    {
      tableData.push_back({
        {std::string(unit->getAbbreviation()), styles::typeName()},
        text_table::Cell(std::string(unit->getName())),
      });
    }

    rows.push_back(ftxui::hbox({ftxui::text("    "), text_table::render(std::move(tableData))}));
    anyOutput = true;
  }

  if (!anyOutput)
  {
    app_.appendInfo(filter.empty() ? "No units registered." : "No units matching '" + filter + "'.");
    return;
  }

  app_.appendElement(ftxui::vbox(std::move(rows)));
}

void CommandEngine::cmdVersion(std::string_view /*args*/)
{
  const auto& build = kernel::Kernel::getBuildInfo();
  auto gitStatusStr = std::string(StringConversionTraits<kernel::GitStatus>::toString(build.gitStatus));

  ftxui::Elements rows;

  rows.push_back(ftxui::text("  Sen") | ftxui::bold);

  {
    auto transportVersion = api_.getTransportProtocolVersion();
    std::string protocols = "kernel: " + std::to_string(kernel::getKernelProtocolVersion());
    if (transportVersion.has_value())
    {
      protocols += ", transport: " + std::to_string(*transportVersion);
    }

    std::string kernelSummary = std::string(SEN_VERSION_STRING) + " " + build.compiler + " [" +
                                (build.debugMode ? "debug" : "release") + "] " + build.buildTime;

    std::vector<text_table::Row> tableData = {
      {text_table::Cell("kernel"), {kernelSummary, styles::mutedText()}},
      {text_table::Cell("branch"), {build.gitRef + " [" + gitStatusStr + "]", styles::mutedText()}},
      {text_table::Cell("commit"), {build.gitHash, styles::mutedText()}},
      {text_table::Cell("protocols"), {protocols, styles::mutedText()}},
    };

    rows.push_back(ftxui::hbox({ftxui::text("    "), text_table::render(std::move(tableData))}));
  }

  auto loadedComponents = api_.getLoadedComponents();
  auto importedPackages = api_.getImportedPackages();

  if (!loadedComponents.empty() || !importedPackages.empty())
  {
    rows.push_back(ftxui::text(""));
    rows.push_back(ftxui::text("  Packages") | ftxui::bold);

    std::vector<text_table::Row> tableData;

    auto appendEntry = [&tableData](std::string_view name, const kernel::BuildInfo& bi)
    {
      tableData.push_back({
        text_table::Cell(std::string(name)),
        text_table::Cell(bi.version),
        {bi.compiler, styles::mutedText()},
        {bi.debugMode ? "debug" : "release", styles::mutedText()},
        {bi.buildTime, styles::mutedText()},
      });
    };

    for (const auto& comp: loadedComponents)
    {
      appendEntry(comp.name, comp.buildInfo);
    }

    if (!loadedComponents.empty() && !importedPackages.empty())
    {
      tableData.emplace_back();
    }

    for (const auto& pkg: importedPackages)
    {
      appendEntry(pkg.name, pkg.buildInfo);
    }

    rows.push_back(ftxui::hbox({ftxui::text("    "), text_table::render(std::move(tableData))}));
  }

  app_.appendElement(ftxui::vbox(std::move(rows)));
}

void CommandEngine::cmdStatus(std::string_view /*args*/)
{
  auto info = api_.fetchMonitoringInfo();

  ftxui::Elements rows;

  // Run mode
  auto runModeStr = std::string(sen::toString(info.runMode));
  rows.push_back(ftxui::hbox({
    ftxui::text("  Run mode  ") | styles::mutedText(),
    ftxui::text(runModeStr),
  }));

  // Transport stats
  const auto& ts = info.transportStats;
  bool hasTransport = (ts.udpSentBytes + ts.udpReceivedBytes + ts.tcpSentBytes + ts.tcpReceivedBytes) > 0;
  if (hasTransport)
  {
    rows.push_back(ftxui::text(""));
    rows.push_back(ftxui::text("  Transport") | ftxui::bold);
    rows.push_back(ftxui::hbox({
      ftxui::text("    UDP  ") | styles::mutedText(),
      ftxui::text("sent "),
      ftxui::text(byte_format::formatBytes(ts.udpSentBytes)) | styles::valueNumber(),
      ftxui::text("  received "),
      ftxui::text(byte_format::formatBytes(ts.udpReceivedBytes)) | styles::valueNumber(),
    }));
    rows.push_back(ftxui::hbox({
      ftxui::text("    TCP  ") | styles::mutedText(),
      ftxui::text("sent "),
      ftxui::text(byte_format::formatBytes(ts.tcpSentBytes)) | styles::valueNumber(),
      ftxui::text("  received "),
      ftxui::text(byte_format::formatBytes(ts.tcpReceivedBytes)) | styles::valueNumber(),
    }));
  }

  // Components table
  if (!info.components.empty())
  {
    rows.push_back(ftxui::text(""));
    rows.push_back(ftxui::text("  Components") | ftxui::bold);

    std::vector<text_table::Row> tableData;
    tableData.push_back({
      {"Name", ftxui::text("Name") | ftxui::bold | styles::mutedText()},
      {"Group", ftxui::text("Group") | ftxui::bold | styles::mutedText()},
      {"Cycle time", ftxui::text("Cycle time") | ftxui::bold | styles::mutedText()},
      {"Objects", ftxui::text("Objects") | ftxui::bold | styles::mutedText()},
      text_table::Cell(""),
    });

    for (const auto& comp: info.components)
    {
      std::string cycleStr = "N/A";
      if (comp.cycleTime.has_value() && comp.cycleTime->toSeconds() > 0.0)
      {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (1.0 / comp.cycleTime->toSeconds()) << " Hz";
        cycleStr = oss.str();
      }

      tableData.push_back({
        text_table::Cell(comp.name),
        text_table::Cell(std::to_string(comp.group)),
        {cycleStr, styles::valueNumber()},
        {std::to_string(comp.objectCount), styles::valueNumber()},
        comp.requiresRealTime ? text_table::Cell("realtime", styles::mutedText()) : text_table::Cell(""),
      });
    }

    rows.push_back(ftxui::hbox({ftxui::text("    "), text_table::render(std::move(tableData))}));
  }

  app_.appendElement(ftxui::vbox(std::move(rows)));
}

}  // namespace sen::components::term
