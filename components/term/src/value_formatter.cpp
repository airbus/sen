// === value_formatter.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "value_formatter.h"

// local
#include "styles.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/variant_type.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Produce an indentation element: 2 spaces per level.
ftxui::Element indent(int level) { return ftxui::text(std::string(checkedConversion<std::size_t>(level) * 2, ' ')); }

/// Tree connector for a child entry: "├ " for intermediate items, "└ " for the last.
ftxui::Element connector(bool isLast)
{
  return ftxui::text(std::string(isLast ? unicode::cornerEnd : unicode::branchTee) + " ") |
         ftxui::color(styles::treeConnector());
}

/// Continuation bar for multi-line nested content: separatorLight or blank for the last item.
ftxui::Element continuation(bool isLast)
{
  if (isLast)
  {
    return ftxui::text("  ");
  }
  return ftxui::separatorLight() | ftxui::color(styles::treeConnector());
}

/// TypeVisitor that builds an ftxui::Element from a Var value.
class ValueWriter: public TypeVisitor
{
  SEN_NOCOPY_NOMOVE(ValueWriter)

public:
  ValueWriter(const Var& value, int indentLevel, bool compact): value_(value), indent_(indentLevel), compact_(compact)
  {
  }

  [[nodiscard]] ftxui::Element result() const { return result_; }

  void apply(const BoolType& /*type*/) override
  {
    auto* val = value_.getIf<bool>();
    result_ = ftxui::text(val != nullptr ? (*val ? "true" : "false") : "?") | styles::valueBool();
  }

  void apply(const Int32Type& /*type*/) override { formatInt<int32_t>(); }
  void apply(const UInt32Type& /*type*/) override { formatInt<uint32_t>(); }
  void apply(const Int64Type& /*type*/) override { formatInt<int64_t>(); }
  void apply(const UInt64Type& /*type*/) override { formatInt<uint64_t>(); }
  void apply(const UInt8Type& /*type*/) override { formatInt<uint8_t>(); }
  void apply(const Int16Type& /*type*/) override { formatInt<int16_t>(); }
  void apply(const UInt16Type& /*type*/) override { formatInt<uint16_t>(); }

  void apply(const Float32Type& /*type*/) override { formatFloat<float>(); }
  void apply(const Float64Type& /*type*/) override { formatFloat<double>(); }

  void apply(const StringType& /*type*/) override
  {
    auto* val = value_.getIf<std::string>();
    if (val != nullptr)
    {
      constexpr std::size_t compactMaxLen = 40;
      if (compact_ && val->size() > compactMaxLen)
      {
        result_ = ftxui::text("\"" + val->substr(0, compactMaxLen) + std::string(unicode::ellipsis) + "\"") |
                  styles::valueString();
      }
      else
      {
        result_ = ftxui::text("\"" + *val + "\"") | styles::valueString();
      }
    }
    else
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
    }
  }

  void apply(const DurationType& /*type*/) override
  {
    auto* val = value_.getIf<Duration>();
    if (val != nullptr)
    {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(3) << val->toSeconds();
      result_ = ftxui::hbox({ftxui::text(oss.str()) | styles::valueNumber(), ftxui::text(" s") | styles::unitLabel()});
    }
    else
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
    }
  }

  void apply(const TimestampType& /*type*/) override
  {
    auto* val = value_.getIf<TimeStamp>();
    if (val != nullptr)
    {
      result_ = ftxui::text(val->toUtcString()) | styles::valueNumber();
    }
    else
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
    }
  }

  void apply(const EnumType& type) override
  {
    const Enumerator* enumerator = nullptr;

    if (auto* str = value_.getIf<std::string>(); str != nullptr)
    {
      enumerator = type.getEnumFromName(*str);
    }
    else
    {
      try
      {
        enumerator = type.getEnumFromKey(value_.getCopyAs<uint32_t>());
      }
      catch (...)
      {
      }
    }

    if (enumerator != nullptr)
    {
      auto entry = ftxui::text(std::string(enumerator->name)) | styles::valueEnum();
      if (!compact_ && !enumerator->description.empty())
      {
        ftxui::Elements parts;
        parts.push_back(entry);
        parts.push_back(ftxui::text(" (" + std::string(enumerator->description) + ")") | styles::descriptionText());
        result_ = ftxui::hbox(std::move(parts));
      }
      else
      {
        result_ = entry;
      }
      return;
    }

    result_ = ftxui::text("?") | styles::valueEmpty();
  }

  void apply(const StructType& type) override
  {
    auto* map = value_.getIf<VarMap>();
    if (map == nullptr || map->empty())
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
      return;
    }

    auto allFields = type.getAllFields();
    std::vector<const StructField*> presentFields;
    for (const auto& field: allFields)
    {
      if (map->find(field.name) != map->end())
      {
        presentFields.push_back(&field);
      }
    }

    ftxui::Elements lines;
    for (std::size_t i = 0; i < presentFields.size(); ++i)
    {
      const auto& field = *presentFields[i];
      auto fieldItr = map->find(field.name);
      const bool isLast = (i + 1 == presentFields.size());
      auto* fieldType = field.type.type();

      if (fieldType->isOptionalType())
      {
        if (fieldItr->second.holds<std::monostate>())
        {
          auto fieldValue = ftxui::text("<empty>") | styles::valueEmpty();
          lines.push_back(ftxui::hbox({indent(indent_),
                                       connector(isLast),
                                       ftxui::text(field.name) | styles::fieldName(),
                                       ftxui::text(": "),
                                       fieldValue}));
          continue;
        }
        const auto* optType = fieldType->asOptionalType();
        fieldType = optType->getType().type();
      }

      bool isComplex = fieldType->isStructType() || fieldType->isSequenceType() || fieldType->isVariantType();

      if (isComplex)
      {
        if (auto* list = fieldItr->second.getIf<VarList>(); list != nullptr && list->empty())
        {
          isComplex = false;
        }
        else if (auto* innerMap = fieldItr->second.getIf<VarMap>(); innerMap != nullptr && innerMap->empty())
        {
          isComplex = false;
        }
      }

      if (isComplex)
      {
        lines.push_back(ftxui::hbox(
          {indent(indent_), connector(isLast), ftxui::text(field.name) | styles::fieldName(), ftxui::text(":")}));
        auto nested = formatValue(fieldItr->second, *field.type, indent_ + 1, compact_);
        lines.push_back(ftxui::hbox({indent(indent_), continuation(isLast), ftxui::text(" "), nested}));
      }
      else
      {
        auto fieldValue = formatValue(fieldItr->second, *field.type, indent_ + 1, compact_);
        lines.push_back(ftxui::hbox({indent(indent_),
                                     connector(isLast),
                                     ftxui::text(field.name) | styles::fieldName(),
                                     ftxui::text(": "),
                                     fieldValue}));
      }
    }

    result_ = ftxui::vbox(std::move(lines));
  }

  void apply(const SequenceType& type) override
  {
    auto* list = value_.getIf<VarList>();
    if (list == nullptr || list->empty())
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
      return;
    }

    if (type.getElementType()->isUInt8Type())
    {
      formatBuffer(*list);
      return;
    }

    bool isComplex = type.getElementType()->isStructType() || type.getElementType()->isSequenceType() ||
                     type.getElementType()->isVariantType();

    int indexWidth = (list->size() < 10) ? 1 : (list->size() < 100) ? 2 : 3;

    ftxui::Elements lines;
    for (std::size_t i = 0; i < list->size(); ++i)
    {
      const bool isLast = (i + 1 == list->size());

      std::ostringstream idx;
      idx << "[" << std::setw(indexWidth) << i << "] ";
      if (isComplex)
      {
        // Render the element value at indent 0. The continuation bar already provides the
        // visual nesting, so the struct's own ├/└ connectors sit directly under the index.
        auto elemValue = formatValue((*list)[i], *type.getElementType(), 0, compact_);
        lines.push_back(
          ftxui::hbox({indent(indent_), connector(isLast), ftxui::text(idx.str()) | styles::listIndex()}));
        lines.push_back(ftxui::hbox({indent(indent_), continuation(isLast), ftxui::text(" "), elemValue}));
      }
      else
      {
        auto elemValue = formatValue((*list)[i], *type.getElementType(), indent_ + 1, compact_);
        lines.push_back(
          ftxui::hbox({indent(indent_), connector(isLast), ftxui::text(idx.str()) | styles::listIndex(), elemValue}));
      }
    }

    result_ = ftxui::vbox(std::move(lines));
  }

  void apply(const QuantityType& type) override
  {
    // Normalise to double (the Var may hold any numeric storage type).
    double numVal = 0.0;
    try
    {
      numVal = value_.getCopyAs<double>();
    }
    catch (...)
    {
      // Empty or non-numeric Var. Leave at 0 (will render as "0").
    }

    std::ostringstream oss;
    // Suppress trailing ".0" for integer-storage quantities.
    if (numVal == std::floor(numVal) && std::abs(numVal) < 1e15)
    {
      oss << checkedConversion<int64_t>(numVal);
    }
    else
    {
      oss << numVal;
    }
    auto entry = ftxui::text(oss.str()) | styles::valueNumber();

    auto unit = type.getUnit();
    if (unit.has_value() && *unit != nullptr)
    {
      ftxui::Elements parts;
      parts.push_back(entry);
      parts.push_back(ftxui::text(" " + std::string((*unit)->getAbbreviation())) | styles::unitLabel());
      entry = ftxui::hbox(std::move(parts));
    }

    result_ = entry;
  }

  void apply(const VariantType& type) override
  {
    if (!value_.holds<KeyedVar>())
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
      return;
    }

    const auto& [typeIndex, elementValue] = value_.get<KeyedVar>();
    const auto& fields = type.getFields();

    if (typeIndex >= fields.size())
    {
      result_ = ftxui::text("<invalid variant>") | styles::valueEmpty();
      return;
    }

    // Bounds-checked above; checkedConversion guards against future type changes.
    const auto& field = fields[checkedConversion<std::size_t>(typeIndex)];
    auto typeName = std::string(field.type->getName());

    ftxui::Elements lines;
    const bool hasValue = (elementValue != nullptr);
    lines.push_back(ftxui::hbox({indent(indent_),
                                 connector(!hasValue),
                                 ftxui::text("type: ") | styles::fieldName(),
                                 ftxui::text(typeName) | styles::typeName()}));

    if (hasValue)
    {
      bool isEmpty = elementValue->holds<std::monostate>();
      if (!isEmpty)
      {
        if (auto* m = elementValue->getIf<VarMap>(); m != nullptr && m->empty())
        {
          isEmpty = true;
        }
        else if (auto* l = elementValue->getIf<VarList>(); l != nullptr && l->empty())
        {
          isEmpty = true;
        }
      }

      bool isComplex =
        !isEmpty && (field.type->isStructType() || field.type->isSequenceType() || field.type->isVariantType());

      auto fieldValue = formatValue(*elementValue, *field.type, indent_ + 1, compact_);
      if (isComplex)
      {
        lines.push_back(ftxui::hbox({indent(indent_), connector(true), ftxui::text("value:") | styles::fieldName()}));
        lines.push_back(ftxui::hbox({indent(indent_), continuation(true), fieldValue}));
      }
      else
      {
        lines.push_back(
          ftxui::hbox({indent(indent_), connector(true), ftxui::text("value: ") | styles::fieldName(), fieldValue}));
      }
    }

    result_ = ftxui::vbox(std::move(lines));
  }

  void apply(const OptionalType& type) override
  {
    if (value_.holds<std::monostate>())
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
      return;
    }
    result_ = formatValue(value_, *type.getType(), indent_, compact_);
  }

  void apply(const AliasType& type) override
  {
    result_ = formatValue(value_, *type.getAliasedType(), indent_, compact_);
  }

  void apply(const VoidType& /*type*/) override { result_ = ftxui::text("<void>") | styles::valueEmpty(); }

  void apply(const Type& /*type*/) override { result_ = ftxui::text(toJson(value_)) | styles::valueNumber(); }

private:
  void formatBuffer(const VarList& bytes)
  {
    constexpr std::size_t bytesPerLine = 16;
    ftxui::Elements lines;

    for (std::size_t i = 0; i < bytes.size(); i += bytesPerLine)
    {
      std::ostringstream hex;
      for (std::size_t j = i; j < std::min(i + bytesPerLine, bytes.size()); ++j)
      {
        if (j > i)
        {
          hex << ' ';
        }
        try
        {
          hex << std::setfill('0') << std::setw(2) << std::hex
              << checkedConversion<unsigned>(bytes[j].getCopyAs<uint8_t>());
        }
        catch (...)
        {
          hex << "??";
        }
      }
      lines.push_back(ftxui::hbox({indent(indent_ + 1), ftxui::text(hex.str()) | styles::valueNumber()}));
    }

    if (lines.empty())
    {
      result_ = ftxui::text("<empty>") | styles::valueEmpty();
    }
    else if (lines.size() == 1)
    {
      result_ = lines[0];
    }
    else
    {
      result_ = ftxui::vbox(std::move(lines));
    }
  }

  template <typename T>
  void formatInt()
  {
    auto* val = value_.getIf<T>();
    if (val != nullptr)
    {
      result_ = ftxui::text(std::to_string(*val)) | styles::valueNumber();
    }
    else
    {
      result_ = ftxui::text(toJson(value_)) | styles::valueNumber();
    }
  }

  template <typename T>
  void formatFloat()
  {
    auto* val = value_.getIf<T>();
    if (val != nullptr)
    {
      std::ostringstream oss;
      oss << *val;
      result_ = ftxui::text(oss.str()) | styles::valueNumber();
    }
    else
    {
      result_ = ftxui::text(toJson(value_)) | styles::valueNumber();
    }
  }

  const Var& value_;
  int indent_;
  bool compact_;
  ftxui::Element result_ = ftxui::text("<empty>") | styles::valueEmpty();
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------------------------------------------------

ftxui::Element formatValue(const Var& value, const Type& type, int indent, bool compact)
{
  ValueWriter writer(value, indent, compact);
  type.accept(writer);
  return writer.result();
}

}  // namespace sen::components::term
