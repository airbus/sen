// === command_engine_inspect.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "command_engine.h"

// local
#include "byte_format.h"
#include "styles.h"
#include "text_table.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/span.h"
#include "sen/core/base/version.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
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
        // flex_shrink: this element gives up its space first when the row is too wide,
        // so the member name/type always renders fully and the description clips gracefully.
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
      bool isVoid = (retName == "void");

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
    auto fields = structType->getAllFields();
    if (!fields.empty())
    {
      sections.push_back(ftxui::text(""));
      sections.push_back(ftxui::text("Fields") | ftxui::bold);
      for (std::size_t i = 0; i < fields.size(); ++i)
      {
        const auto& field = fields[i];
        const bool isLast = (i + 1 == fields.size());
        const std::string connector = std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";
        ftxui::Elements row = {ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                               ftxui::text(field.name) | ftxui::bold,
                               ftxui::text(" : ") | styles::mutedText(),
                               ftxui::text(std::string(field.type->getName())) | styles::typeName()};
        if (!field.description.empty())
        {
          row.push_back(ftxui::text("  " + field.description) | styles::mutedText() | ftxui::flex_shrink);
        }
        sections.push_back(ftxui::hbox(std::move(row)));
      }
    }
  }
  else if (auto* enumType = type.asEnumType(); enumType != nullptr)
  {
    auto enums = enumType->getEnums();
    if (!enums.empty())
    {
      sections.push_back(ftxui::text(""));
      sections.push_back(ftxui::text("Values") | ftxui::bold);
      for (std::size_t i = 0; i < enums.size(); ++i)
      {
        const bool isLast = (i + 1 == enums.size());
        const std::string connector = std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";
        ftxui::Elements row = {ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                               ftxui::text(enums[i].name) | ftxui::bold,
                               ftxui::text(" = ") | styles::mutedText(),
                               ftxui::text(std::to_string(enums[i].key)) | styles::typeName()};
        if (!enums[i].description.empty())
        {
          row.push_back(ftxui::text("  " + enums[i].description) | styles::mutedText() | ftxui::flex_shrink);
        }
        sections.push_back(ftxui::hbox(std::move(row)));
      }
    }
  }
  else if (auto* classType = type.asClassType(); classType != nullptr)
  {
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
        if (prop.getCategory() == PropertyCategory::dynamicRW)
        {
          annotation = " [writable]";
        }
        else if (prop.getCategory() == PropertyCategory::staticRO)
        {
          annotation = " [static]";
        }
        sections.push_back(ftxui::hbox({ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                                        ftxui::text(std::string(prop.getName())) | ftxui::bold,
                                        ftxui::text(" : ") | styles::mutedText(),
                                        ftxui::text(std::string(prop.getType()->getName())) | styles::typeName(),
                                        ftxui::text(annotation) | styles::mutedText()}));
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
        ftxui::Elements row = {ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                               ftxui::text(sig) | ftxui::bold};
        if (retName != "void")
        {
          row.push_back(ftxui::text(std::string(" ") + unicode::arrowRight + " ") | styles::mutedText());
          row.push_back(ftxui::text(retName) | styles::typeName());
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
        sections.push_back(ftxui::hbox(
          {ftxui::text(connector) | ftxui::color(styles::treeConnector()), ftxui::text(sig) | ftxui::bold}));
      }
    }
  }
  else if (auto* seqType = type.asSequenceType(); seqType != nullptr)
  {
    sections.push_back(ftxui::text(""));
    sections.push_back(
      ftxui::hbox({ftxui::text("Element type: ") | styles::mutedText(),
                   ftxui::text(std::string(seqType->getElementType()->getName())) | styles::typeName()}));
    if (auto max = seqType->getMaxSize(); max.has_value())
    {
      sections.push_back(
        ftxui::hbox({ftxui::text("Max size: ") | styles::mutedText(), ftxui::text(std::to_string(*max))}));
    }
    if (seqType->hasFixedSize())
    {
      sections.push_back(ftxui::text("Fixed size (array)") | styles::mutedText());
    }
  }
  else if (auto* variantType = type.asVariantType(); variantType != nullptr)
  {
    auto fields = variantType->getFields();
    if (!fields.empty())
    {
      sections.push_back(ftxui::text(""));
      sections.push_back(ftxui::text("Alternatives") | ftxui::bold);
      for (std::size_t i = 0; i < fields.size(); ++i)
      {
        const bool isLast = (i + 1 == fields.size());
        const std::string connector = std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ";
        sections.push_back(ftxui::hbox({ftxui::text(connector) | ftxui::color(styles::treeConnector()),
                                        ftxui::text(std::string(fields[i].type->getName())) | styles::typeName()}));
      }
    }
  }
  else if (auto* quantityType = type.asQuantityType(); quantityType != nullptr)
  {
    sections.push_back(ftxui::text(""));
    sections.push_back(
      ftxui::hbox({ftxui::text("Storage: ") | styles::mutedText(),
                   ftxui::text(std::string(quantityType->getElementType()->getName())) | styles::typeName()}));
    if (auto unit = quantityType->getUnit(); unit.has_value() && *unit != nullptr)
    {
      sections.push_back(ftxui::hbox(
        {ftxui::text("Unit: ") | styles::mutedText(),
         ftxui::text(std::string((*unit)->getName()) + " (" + std::string((*unit)->getAbbreviation()) + ")")}));
    }
    if (auto min = quantityType->getMinValue(); min.has_value())
    {
      sections.push_back(ftxui::hbox({ftxui::text("Min: ") | styles::mutedText(), ftxui::text(std::to_string(*min))}));
    }
    if (auto max = quantityType->getMaxValue(); max.has_value())
    {
      sections.push_back(ftxui::hbox({ftxui::text("Max: ") | styles::mutedText(), ftxui::text(std::to_string(*max))}));
    }
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

  auto typeKind = [](const Type* type) -> std::string
  {
    if (type->asClassType() != nullptr)
      return "class";
    if (type->asStructType() != nullptr)
      return "struct";
    if (type->asEnumType() != nullptr)
      return "enum";
    if (type->asSequenceType() != nullptr)
      return "sequence";
    if (type->asVariantType() != nullptr)
      return "variant";
    if (type->asQuantityType() != nullptr)
      return "quantity";
    if (type->asAliasType() != nullptr)
      return "alias";
    if (type->asOptionalType() != nullptr)
      return "optional";
    return "type";
  };

  if (names.empty())
  {
    rows.push_back(ftxui::text("  (no matching types)") | styles::mutedText());
    app_.appendElement(ftxui::vbox(std::move(rows)));
    return;
  }

  std::vector<textTable::Row> tableData;
  for (const auto& name: names)
  {
    tableData.push_back({
      {name, ftxui::bold},
      {typeKind(allTypes.at(name).type()), styles::mutedText()},
    });
  }

  rows.push_back(ftxui::hbox({ftxui::text("  "), textTable::render(std::move(tableData))}));

  app_.appendElement(ftxui::vbox(std::move(rows)));
}

void CommandEngine::cmdUnits(std::string_view args)
{
  const auto& registry = UnitRegistry::get();

  // Collect all units grouped by category.
  static constexpr UnitCategory categories[] = {
    UnitCategory::length,
    UnitCategory::mass,
    UnitCategory::time,
    UnitCategory::angle,
    UnitCategory::temperature,
    UnitCategory::density,
    UnitCategory::pressure,
    UnitCategory::area,
    UnitCategory::force,
    UnitCategory::frequency,
    UnitCategory::velocity,
    UnitCategory::angularVelocity,
    UnitCategory::acceleration,
    UnitCategory::angularAcceleration,
  };

  std::string filter = std::string(args);

  ftxui::Elements rows;
  bool anyOutput = false;

  for (auto cat: categories)
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

    std::vector<textTable::Row> tableData;
    for (const auto* unit: units)
    {
      tableData.push_back({
        {std::string(unit->getAbbreviation()), styles::typeName()},
        textTable::Cell(std::string(unit->getName())),
      });
    }

    rows.push_back(ftxui::hbox({ftxui::text("    "), textTable::render(std::move(tableData))}));
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

    std::vector<textTable::Row> tableData = {
      {textTable::Cell("kernel"), {kernelSummary, styles::mutedText()}},
      {textTable::Cell("branch"), {build.gitRef + " [" + gitStatusStr + "]", styles::mutedText()}},
      {textTable::Cell("commit"), {build.gitHash, styles::mutedText()}},
      {textTable::Cell("protocols"), {protocols, styles::mutedText()}},
    };

    rows.push_back(ftxui::hbox({ftxui::text("    "), textTable::render(std::move(tableData))}));
  }

  auto loadedComponents = api_.getLoadedComponents();
  auto importedPackages = api_.getImportedPackages();

  if (!loadedComponents.empty() || !importedPackages.empty())
  {
    rows.push_back(ftxui::text(""));
    rows.push_back(ftxui::text("  Packages") | ftxui::bold);

    std::vector<textTable::Row> tableData;

    auto appendEntry = [&tableData](std::string_view name, const kernel::BuildInfo& bi)
    {
      tableData.push_back({
        textTable::Cell(std::string(name)),
        textTable::Cell(bi.version),
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

    rows.push_back(ftxui::hbox({ftxui::text("    "), textTable::render(std::move(tableData))}));
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

    std::vector<textTable::Row> tableData;
    tableData.push_back({
      {"Name", ftxui::text("Name") | ftxui::bold | styles::mutedText()},
      {"Group", ftxui::text("Group") | ftxui::bold | styles::mutedText()},
      {"Cycle time", ftxui::text("Cycle time") | ftxui::bold | styles::mutedText()},
      {"Objects", ftxui::text("Objects") | ftxui::bold | styles::mutedText()},
      textTable::Cell(""),
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
        textTable::Cell(comp.name),
        textTable::Cell(std::to_string(comp.group)),
        {cycleStr, styles::valueNumber()},
        {std::to_string(comp.objectCount), styles::valueNumber()},
        comp.requiresRealTime ? textTable::Cell("realtime", styles::mutedText()) : textTable::Cell(""),
      });
    }

    rows.push_back(ftxui::hbox({ftxui::text("    "), textTable::render(std::move(tableData))}));
  }

  app_.appendElement(ftxui::vbox(std::move(rows)));
}

}  // namespace sen::components::term
