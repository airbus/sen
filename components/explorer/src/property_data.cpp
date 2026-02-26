// === property_data.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "editable_printer_maker.h"

#ifdef WIN32
#  define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "property_data.h"

// component
#include "object_state.h"
#include "plotter.h"
#include "value.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/detail/work_queue.h"

// imgui
#include "imgui.h"

// std
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <ios>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr uint32_t sequenceElementSeed = 74;
constexpr uint32_t structElementSeed = 75;
constexpr uint32_t variantElementSeed = 76;
constexpr uint32_t maxAge = 10U;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

/// Formats a list of bytes into a string of their Ascii characters.
///
/// Example:
///   Input: A VarList containing the byte values {72 101 108 108 111}
///   Returns: "Hello"
std::string formatBytesAsAscii(const sen::VarList& list)
{
  std::string string;
  for (const auto& elem: list)
  {
    if (elem.holds<uint8_t>())
    {
      string.push_back(static_cast<char>(elem.getCopyAs<uint8_t>()));
    }
    else
    {
      string.append("? ");
    }
  }
  return string;
}

/// Formats a list of bytes into a string of their hexadecimal values.
///
/// Example:
///   Input: A VarList containing the byte values {72 101 108 108 111}
///   Returns: "48 65 6C 6C 6F"
std::string formatBytesAsHex(const sen::VarList& list)
{
  std::stringstream hexStream;
  hexStream << std::hex << std::uppercase;
  for (const auto& elem: list)
  {
    if (elem.holds<uint8_t>())
    {
      hexStream << std::setfill('0') << std::setw(2) << static_cast<int>(elem.getCopyAs<uint8_t>()) << " ";
    }
    else
    {
      hexStream << "? ";
    }
  }
  return hexStream.str();
}

/// Formats a list of bytes into a string of their decimal values.
///
/// Example:
///   Input: A VarList containing the byte values {72 101 108 108 111}
///   Returns: "72 101 108 108 111"
std::string formatBytesAsDecimal(const sen::VarList& list)
{
  std::string string;
  for (const auto& elem: list)
  {
    if (elem.holds<uint8_t>())
    {
      string.append(elem.getCopyAs<std::string>()).append(" ");
    }
    else
    {
      string.append("? ");
    }
  }
  return string;
}

enum class ViewMode : uint8_t
{
  ascii,
  hex,
  decimal,
};

//--------------------------------------------------------------------------------------------------------------
// Types and forward declarations
//--------------------------------------------------------------------------------------------------------------

PrinterFunc getOrCreatePrinter(PrinterRegistry& reg,
                               sen::ConstTypeHandle<sen::Type> type,
                               bool useTreesForCompounds = true);
ReporterFunc getOrCreateReporter(ReporterRegistry& reg, sen::ConstTypeHandle<sen::Type> type);

//--------------------------------------------------------------------------------------------------------------
// PrinterMaker
//--------------------------------------------------------------------------------------------------------------

/// When false the capability to drag fields to the plot will be disabled
bool dragEnabled = true;

class PrinterMaker final: public sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(PrinterMaker)

public:
  PrinterMaker(bool useTrees, PrinterRegistry& printerRegistry): useTrees_(useTrees), printerRegistry_(printerRegistry)
  {
  }

  ~PrinterMaker() override = default;

  [[nodiscard]] PrinterFunc getResult() const noexcept { return func_; }

public:
  void apply(const sen::EnumType& type) override
  {
    func_ = [&type](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printEnum(value.getCopyAs<uint32_t>(), type);
    };
  }

  void apply(const sen::BoolType& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printBool(value.get<bool>());
    };
  }

  void apply(const sen::UInt8Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printIntegral(value.get<uint8_t>());
    };
  }

  void apply(const sen::Int16Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printIntegral(value.get<int16_t>());
    };
  }

  void apply(const sen::UInt16Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printIntegral(value.get<uint16_t>());
    };
  }

  void apply(const sen::Int32Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printIntegral(value.get<int32_t>());
    };
  }

  void apply(const sen::UInt32Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printIntegral(value.get<uint32_t>());
    };
  }

  void apply(const sen::Int64Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printIntegral(value.get<int64_t>());
    };
  }

  void apply(const sen::UInt64Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printIntegral(value.get<uint64_t>());
    };
  }

  void apply(const sen::Float32Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printFloat(value.get<float32_t>());
    };
  }

  void apply(const sen::Float64Type& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printFloat(value.get<float64_t>());
    };
  }

  void apply(const sen::TimestampType& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      enableDrag(id, state, prefix);
      Value::printTime(value.getCopyAs<sen::TimeStamp>());
    };
  }

  void apply(const sen::StringType& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t /*id*/, const std::string& /*prefix*/, ObjectState* /*state*/)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
        return;
      }

      Value::printString(value.get<std::string>());
    };
  }

  void apply(const sen::StructType& type) override
  {
    const auto fields = type.getFields();
    const auto fieldCount = fields.size();

    std::vector<PrinterFunc> fieldsPrinters;
    {
      fieldsPrinters.reserve(fieldCount);
      for (const auto& field: fields)
      {
        fieldsPrinters.emplace_back(
          [fieldPrinter = getOrCreatePrinter(printerRegistry_, field.type, useTrees_),
           fieldName = field.name.c_str(),
           fieldNameCrc = sen::crc32(field.name),
           fieldDescription = field.description.data()](
            const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
          {
            Value::writeName(fieldName, fieldDescription);
            if (!value.holds<sen::VarMap>())
            {
              return;
            }

            const auto& map = value.get<sen::VarMap>();
            auto valueItr = map.find(fieldName);
            const auto fieldId = sen::hashCombine(structElementSeed, id, fieldNameCrc);

            std::string fieldPrefix = prefix;
            fieldPrefix.append(".");
            fieldPrefix.append(fieldName);

            fieldPrinter((valueItr == map.end() ? sen::Var {} : valueItr->second), fieldId, fieldPrefix, state);
          });
      }
    }

    std::string title;
    title.append(type.getQualifiedName());
    title.append(" (");
    title.append(std::to_string(fieldCount));
    title.append(fieldCount == 1U ? " field" : " fields");
    title.append(")");

    func_ = [printers = std::move(fieldsPrinters), theTitle = std::move(title), useTrees = useTrees_](
              const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      ImGui::PushID(static_cast<int>(id));
      ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);

      std::string treeLabel = std::string("##struct_tree_") + std::to_string(id);
      std::string tableLabel = std::string("##struct_table_") + std::to_string(id);

      if (useTrees && !ImGui::TreeNode(treeLabel.c_str(), "%s", theTitle.c_str()))  // NOLINT
      {
        ImGui::PopID();
        return;
      }

      if (ImGui::BeginTable(tableLabel.c_str(), 2, Value::tableFlags))
      {
        Value::setupColumns();

        for (const auto& printer: printers)
        {
          ImGui::TableNextRow();
          printer(value, id, prefix, state);
        }
        ImGui::EndTable();
      }

      if (useTrees)
      {
        ImGui::TreePop();
      }

      ImGui::PopID();
    };
  }

  void apply(const sen::VariantType& type) override
  {
    std::map<sen::MemberHash, PrinterFunc> fieldsPrinter;
    {
      const auto fields = type.getFields();
      for (const auto& field: fields)
      {
        fieldsPrinter[field.key] =
          [fieldPrinter = getOrCreatePrinter(printerRegistry_, field.type, useTrees_), fieldId = field.key](
            const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
        {
          auto fieldKey = sen::hashCombine(variantElementSeed, fieldId, id);
          fieldPrinter(value, fieldKey, prefix, state);
        };
      }
    }

    func_ = [thePrinter = std::move(fieldsPrinter), variantType = &type](
              const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      ImGui::PushID(static_cast<int>(id));

      if (value.holds<sen::KeyedVar>())
      {
        const auto& [fieldKey, fieldValue] = value.get<sen::KeyedVar>();

        auto updaterItr = thePrinter.find(fieldKey);
        if (updaterItr != thePrinter.end())
        {
          updaterItr->second(*fieldValue, id, prefix, state);
        }
        else
        {
          std::string err;
          err.append("fieldsPrinter does not hold any element for key ");
          err.append(std::to_string(fieldKey));
          err.append(" of Variant '");
          err.append(variantType->getQualifiedName());
          err.append("'");
          sen::throwRuntimeError(err);
        }
      }
      else
      {
        std::string err;
        err.append("Var expected to hold KeyedVar but holds type with index ");
        err.append(std::to_string(static_cast<const ::sen::Var::ValueType&>(value).index()));
        sen::throwRuntimeError(err);
      }
      ImGui::PopID();
    };
  }

  void apply(const sen::SequenceType& type) override
  {
    static const auto stringColor = Value::getStringColor();

    if (type.getElementType()->isUInt8Type())
    {
      func_ = [](const sen::Var& value, uint32_t id, const std::string& /*prefix*/, ObjectState* /*state*/)
      {
        ImGui::PushID(static_cast<int>(id));

        static std::map<uint32_t, ViewMode> viewModeMap;
        if (viewModeMap.find(id) == viewModeMap.end())
        {
          viewModeMap[id] = ViewMode::ascii;
        }

        ViewMode* currentViewMode = &viewModeMap[id];
        int intMode = static_cast<int>(*currentViewMode);
        ImGui::PushItemWidth(80);
        ImGui::Combo("###view", &intMode, "Text\0Hex\0Decimal\0");
        ImGui::PopItemWidth();
        *currentViewMode = static_cast<ViewMode>(intMode);
        ImGui::SameLine();

        if (const auto& list = value.get<sen::VarList>(); list.empty())
        {
          ImGui::TextColored(stringColor, "<empty>");  // NOLINT
        }
        else
        {
          constexpr auto maxCharsToDraw = 128;
          if (list.size() < maxCharsToDraw)
          {
            std::string string;
            switch (*currentViewMode)
            {
              case ViewMode::ascii:
                string = formatBytesAsAscii(list);
                break;
              case ViewMode::hex:
                string = formatBytesAsHex(list);
                break;
              case ViewMode::decimal:
                string = formatBytesAsDecimal(list);
                break;
              default:
                assert(false && "Unknown view mode, would default to an ascii text interpretation.");
            }

            ImGui::TextColored(stringColor, "%s", string.c_str());  // NOLINT
          }
          else
          {
            ImGui::TextColored(stringColor, "%zu bytes", list.size());  // NOLINT
          }
        }
        ImGui::PopID();
      };
    }
    else
    {
      const bool elementUseTrees = type.getElementType()->isStructType() || type.getElementType()->isVariantType();

      func_ = [valuePrinter = getOrCreatePrinter(printerRegistry_, type.getElementType(), elementUseTrees),
               elementTypeId = sen::crc32(type.getElementType()->getName())](
                const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
      {
        std::ignore = state;
        const auto elementId = static_cast<uint32_t>(sen::hashCombine(sequenceElementSeed, id, elementTypeId));

        ImGui::PushID(static_cast<int>(id));
        const auto& list = value.get<sen::VarList>();

        dragEnabled = false;
        if (const char* suffix = list.size() == 1U ? "element" : "elements";
            ImGui::TreeNode("##value", "%zu %s", list.size(), suffix))  // NOLINT
        {
          size_t index = 0;
          for (const auto& elem: list)
          {
            valuePrinter(elem, sen::hashCombine(elementId, index++), prefix, nullptr);
          }
          ImGui::TreePop();
        }
        dragEnabled = true;
        ImGui::PopID();
      };
    }
  }

  void apply(const sen::QuantityType& type) override
  {
    static const auto numberColor = Value::getNumberColor();
    const auto baseUnit = type.getUnit();
    const auto* integral = type.getElementType()->asIntegralType();

    if (!baseUnit)
    {
      func_ = [integral](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
      {
        if (value.isEmpty())
        {
          ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
          return;
        }

        enableDrag(id, state, prefix);

        if (integral)
        {
          if (integral->isSigned())
          {
            ImGui::TextColored(numberColor, "%ld", value.getCopyAs<int64_t>());  // NOLINT
          }
          else
          {
            ImGui::TextColored(numberColor, "%lu", value.getCopyAs<uint64_t>());  // NOLINT
          }
        }
        else
        {
          ImGui::TextColored(numberColor, "%f", value.getCopyAs<float64_t>());  // NOLINT
        }
      };
    }
    else
    {
      func_ = [baseUnit = baseUnit.value(), integral](
                const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
      {
        if (value.isEmpty())
        {
          ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT
          return;
        }

        enableDrag(id, state, prefix);

        static std::map<uint32_t, const sen::Unit*> selectedUnitMap;
        if (selectedUnitMap.find(id) == selectedUnitMap.end())
        {
          selectedUnitMap[id] = baseUnit;
        }

        const sen::Unit* targetUnit = selectedUnitMap[id];

        if (targetUnit == baseUnit)
        {
          if (integral)
          {
            if (integral->isSigned())
            {
              ImGui::TextColored(numberColor, "%ld", value.getCopyAs<int64_t>());  // NOLINT
            }
            else
            {
              ImGui::TextColored(numberColor, "%lu", value.getCopyAs<uint64_t>());  // NOLINT
            }
          }
          else
          {
            ImGui::TextColored(numberColor, "%f", value.getCopyAs<float64_t>());  // NOLINT
          }
        }
        else
        {
          auto valNative = value.getCopyAs<float64_t>();
          auto valueDisplayed = sen::Unit::convert(*baseUnit, *targetUnit, valNative);

          ImGui::TextColored(numberColor, "%f", valueDisplayed);  // NOLINT
        }

        ImGui::PushID(static_cast<int>(id));
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 2.0f));
        if (ImGui::BeginCombo("###unit_combo", targetUnit->getAbbreviation().data(), ImGuiComboFlags_WidthFitPreview))
        {
          auto availableUnits = sen::UnitRegistry::get().getUnitsByCategory(baseUnit->getCategory());
          for (const auto unit: availableUnits)
          {
            bool isSelected = (unit == targetUnit);

            std::string label = std::string(unit->getAbbreviation()) + "(" + std::string(unit->getName()) + ")";

            if (ImGui::Selectable(label.c_str(), isSelected))
            {
              selectedUnitMap[id] = unit;
            }

            if (isSelected)
            {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
      };
    }
  }

  void apply(const sen::AliasType& type) override { type.getAliasedType()->accept(*this); }

  void apply(const sen::OptionalType& type) override
  {
    type.getType()->accept(*this);

    auto underlyingFunc = func_;
    func_ = [underlyingFunc](const sen::Var& value, uint32_t id, const std::string& prefix, ObjectState* state)
    {
      if (value.isEmpty())
      {
        ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT (hicpp-vararg)
        return;
      }
      underlyingFunc(value, id, prefix, state);
    };
  }

private:
  static void enableDrag(uint32_t id, ObjectState* state, const std::string& label)
  {
    if (!dragEnabled)
    {
      return;
    }

    ImGui::PushID(static_cast<int>(id));
    ImGui::SmallButton(" ");
    if (ImGui::BeginDragDropSource())
    {
      PropStateDrag payload {id, state, {}};

      constexpr auto lastIndex = sizeof(payload.elementLabel) - 1;
      strncpy(payload.elementLabel, label.c_str(), lastIndex);
      payload.elementLabel[lastIndex] = '\0';

      ImGui::SetDragDropPayload(PropStateDrag::getTypeString(), &payload, sizeof(payload), ImGuiCond_None);
      ImGui::TextUnformatted(payload.elementLabel);

      ImGui::EndDragDropSource();
    }
    ImGui::PopID();
    ImGui::SameLine();
  }

private:
  PrinterFunc func_ = nullptr;
  bool useTrees_ = true;
  PrinterRegistry& printerRegistry_;
};

PrinterFunc getOrCreatePrinter(PrinterRegistry& reg, sen::ConstTypeHandle<sen::Type> type, bool useTreesForCompounds)
{
  if (auto itr = reg.find(type); itr != reg.end())
  {
    return itr->second;
  }

  PrinterMaker visitor(useTreesForCompounds, reg);
  type->accept(visitor);

  auto [loc, done] = reg.try_emplace(type, visitor.getResult());
  return loc->second;
}

//--------------------------------------------------------------------------------------------------------------
// ReporterMaker
//--------------------------------------------------------------------------------------------------------------

class ReporterMaker final: public sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(ReporterMaker)

public:
  explicit ReporterMaker(ReporterRegistry& reporterRegistry): reporterRegistry_(reporterRegistry) {}

  ~ReporterMaker() override = default;

  [[nodiscard]] ReporterFunc getResult() const noexcept { return func_; }

public:
  void apply(const sen::EnumType& type) override
  {
    std::ignore = type;

    func_ = [](const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
    {
      if (value.isEmpty())
      {
        return;
      }

      auto itr = dataMap.find(id);
      if (itr != dataMap.end())
      {
        for (auto* plot: itr->second)
        {
          plot->addPoint(time, value.getCopyAs<float64_t>());
        }
      }
    };
  }

  void apply(const sen::BoolType& type) override
  {
    std::ignore = type;

    func_ = [](const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
    {
      if (value.isEmpty())
      {
        return;
      }

      auto itr = dataMap.find(id);
      if (itr != dataMap.end())
      {
        for (auto* plot: itr->second)
        {
          plot->addPoint(time, value.get<bool>() ? 1.0 : 0.0);
        }
      }
    };
  }

  void apply(const sen::UInt8Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<uint8_t>();
  }

  void apply(const sen::Int16Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<int16_t>();
  }

  void apply(const sen::UInt16Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<uint16_t>();
  }

  void apply(const sen::Int32Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<int32_t>();
  }

  void apply(const sen::UInt32Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<uint32_t>();
  }

  void apply(const sen::Int64Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<int64_t>();
  }

  void apply(const sen::UInt64Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<uint64_t>();
  }

  void apply(const sen::Float32Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<float32_t>();
  }

  void apply(const sen::Float64Type& type) override
  {
    std::ignore = type;
    func_ = extractNative<float64_t>();
  }

  void apply(const sen::TimestampType& type) override
  {
    std::ignore = type;
    func_ = [](const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
    {
      if (value.isEmpty())
      {
        return;
      }

      auto itr = dataMap.find(id);
      if (itr != dataMap.end())
      {
        auto native = value.getCopyAs<sen::TimeStamp>();

        for (auto* plot: itr->second)
        {
          plot->addPoint(time, native.sinceEpoch().toSeconds());
        }
      }
    };
  }

  void apply(const sen::StructType& type) override
  {
    const auto fields = type.getFields();

    std::vector<ReporterFunc> fieldsReporters;
    {
      fieldsReporters.reserve(fields.size());
      for (const auto& field: fields)
      {
        fieldsReporters.emplace_back(
          [fieldReporter = getOrCreateReporter(reporterRegistry_, field.type),
           fieldName = field.name.c_str(),
           fieldNameCrc = sen::crc32(field.name)](
            const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
          {
            if (!value.holds<sen::VarMap>())
            {
              return;
            }

            const auto& map = value.get<sen::VarMap>();
            auto valueItr = map.find(fieldName);
            const auto fieldId = static_cast<uint32_t>(sen::hashCombine(structElementSeed, id, fieldNameCrc));
            const auto& fieldValue = (valueItr == map.end() ? sen::Var {} : valueItr->second);

            fieldReporter(fieldValue, fieldId, time, dataMap);
          });
      }
    }

    func_ = [reporters = std::move(fieldsReporters)](
              const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
    {
      for (const auto& reporter: reporters)
      {
        reporter(value, id, time, dataMap);
      }
    };
  }

  void apply(const sen::VariantType& type) override
  {
    std::map<sen::MemberHash, ReporterFunc> fieldsReporters;
    {
      const auto fields = type.getFields();
      for (const auto& field: fields)
      {
        fieldsReporters[field.key] =
          [fieldReporter = getOrCreateReporter(reporterRegistry_, field.type), fieldId = field.key](
            const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
        {
          auto fieldKey = static_cast<uint32_t>(sen::hashCombine(variantElementSeed, fieldId, id));
          fieldReporter(value, fieldKey, time, dataMap);
        };
      }
    }

    func_ = [reporters = std::move(fieldsReporters)](
              const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
    {
      if (value.isEmpty())
      {
        return;
      }

      if (value.holds<sen::KeyedVar>())
      {
        const auto& [key, val] = value.get<sen::KeyedVar>();

        auto reporterItr = reporters.find(key);
        if (reporterItr != reporters.end())
        {
          reporterItr->second(*val, id, time, dataMap);
        }
      }
    };
  }

  void apply(const sen::QuantityType& type) override
  {
    if (type.getElementType()->isIntegralType())
    {
      if (type.getElementType()->isSigned())
      {
        func_ = extractNativeAsCopy<int32_t>();
      }
      else
      {
        func_ = extractNativeAsCopy<uint32_t>();
      }
    }
    else
    {
      func_ = extractNativeAsCopy<float64_t>();
    }
  }

  void apply(const sen::AliasType& type) override { type.getAliasedType()->accept(*this); }

  void apply(const sen::OptionalType& type) override { type.getType()->accept(*this); }

private:
  template <typename T>
  static ReporterFunc extractNative()
  {
    return [](const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
    {
      if (value.isEmpty())
      {
        return;
      }

      auto itr = dataMap.find(id);
      if (itr != dataMap.end())
      {
        auto native = value.get<T>();
        for (auto* plot: itr->second)
        {
          plot->addPoint(time, static_cast<float64_t>(native));
        }
      }
    };
  }

  template <typename T>
  static ReporterFunc extractNativeAsCopy()
  {
    return [](const sen::Var& value, uint32_t id, const sen::TimeStamp& time, ObservedDataMap& dataMap)
    {
      if (value.isEmpty())
      {
        return;
      }

      auto itr = dataMap.find(id);
      if (itr != dataMap.end())
      {
        auto native = value.getCopyAs<T>();
        for (auto* plot: itr->second)
        {
          plot->addPoint(time, static_cast<float64_t>(native));
        }
      }
    };
  }

private:
  ReporterFunc func_ = [](const sen::Var&, uint32_t, const sen::TimeStamp&, ObservedDataMap&)
  {
    // no code needed.
  };

  ReporterRegistry& reporterRegistry_;
};

ReporterFunc getOrCreateReporter(ReporterRegistry& reg, sen::ConstTypeHandle<sen::Type> type)
{
  if (auto itr = reg.find(type); itr != reg.end())
  {
    return itr->second;
  }

  ReporterMaker visitor(reg);
  type->accept(visitor);

  auto [loc, done] = reg.try_emplace(type, visitor.getResult());
  return loc->second;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// PropertyData
//--------------------------------------------------------------------------------------------------------------

const char* PropStateDrag::getTypeString() noexcept { return "sen:expl:prop"; }

PropertyData::PropertyData(const sen::Property* prop,
                           ObjectState* owner,
                           sen::impl::WorkQueue* queue,
                           sen::Var initialValue,
                           PrinterRegistry& printerRegistry,
                           ReporterRegistry& reporterRegistry)
  : owner_(owner)
  , prop_(prop)
  , hash_(sen::hashCombine(12150783, prop->getId().get(), sen::crc32(owner->getInstance()->getLocalName())))
  , value_(std::move(initialValue))
  , printer_(getOrCreatePrinter(printerRegistry, prop->getType()))
  , printerEditable_(getOrCreateEditablePrinter(queue, &propertiesStateMap_, *prop->getType(), prop))
  , reporter_(getOrCreateReporter(reporterRegistry, prop->getType()))
{
  prefix_ = owner->getInstance()->getLocalName();
  prefix_.append(".");
  prefix_.append(prop_->getName());
}

PropertyData::~PropertyData() = default;

void PropertyData::changed(ObservedDataMap& observedData)
{
  age_ = maxAge;
  value_ = owner_->getInstance()->getPropertyUntyped(prop_);
  reporter_(value_, hash_, owner_->getInstance()->getLastCommitTime(), observedData);
}

void PropertyData::update()
{
  const bool isYoung = (age_ != 0U);
  if (isYoung)
  {
    const auto step = static_cast<float>(maxAge - age_) / static_cast<float>(maxAge);
    const auto c = 0.5f + 0.5f * step;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, c, c, 1.0));
  }
  const auto writable = prop_->getCategory() == sen::PropertyCategory::dynamicRW;
  Value::writeName(prop_->getName().data(), prop_->getDescription().data());
  if (ImGui::BeginTable("propTable", writable ? 2 : 1))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    printer_(value_, hash_, prefix_, owner_);
    if (writable)
    {
      ImGui::TableNextColumn();
      printerEditable_(value_, prefix_, owner_->getInstance());
    }
    ImGui::EndTable();
  }
  ImGui::Separator();

  if (isYoung)
  {
    ImGui::PopStyleColor();
    age_--;
  }
}
