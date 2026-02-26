// === quantity_visitor.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "quantity_visitor.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"

// imgui
#include "imgui.h"

// std
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

QuantityVisitor::QuantityVisitor(std::string unitName,
                                 std::string quantityName,
                                 bool bounded,
                                 std::optional<float64_t> lowerBound,
                                 std::optional<float64_t> upperBound)
  : unitName(std::move(unitName))
  , quantityName(std::move(quantityName))
  , bounded_(bounded)
  , min_(lowerBound)
  , max_(upperBound)
{
}

QuantityVisitor::QuantityVisitor(const sen::QuantityType& quantityType)
  : unitName(quantityType.getUnit() ? quantityType.getUnit(sen::Unit::ensurePresent).getAbbreviation().data() : "")
  , quantityName(quantityType.getName().data())
  , bounded_(quantityType.isBounded())
  , min_(quantityType.getMinValue())
  , max_(quantityType.getMaxValue())
{
}

/// Visitor functions
void QuantityVisitor::apply(const sen::IntegralType& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<int64_t>(ImGuiDataType_S64, "%ld", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<int64_t>(ImGuiDataType_S64, "%ld", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::RealType& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<float64_t>(ImGuiDataType_Double, "%4.6f", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<float64_t>(ImGuiDataType_Double, "%4.6f", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::UInt8Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<uint8_t>(ImGuiDataType_U8, "%u", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<uint8_t>(ImGuiDataType_U8, "%u", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::UInt16Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<uint16_t>(ImGuiDataType_U16, "%u", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<uint16_t>(ImGuiDataType_U16, "%u", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::UInt32Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<uint32_t>(ImGuiDataType_U32, "%u", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<uint32_t>(ImGuiDataType_U32, "%u", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::UInt64Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<uint64_t>(ImGuiDataType_U64, "%lu", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<uint32_t>(ImGuiDataType_U64, "%lu", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::Int16Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<int16_t>(ImGuiDataType_S16, "%d", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<int16_t>(ImGuiDataType_S16, "%d", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::Int32Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<int32_t>(ImGuiDataType_S32, "%d", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<int32_t>(ImGuiDataType_S32, "%d", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::Int64Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<int64_t>(ImGuiDataType_S64, "%ld", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<int32_t>(ImGuiDataType_S64, "%ld", unitName, quantityName);
  }
}

void QuantityVisitor::apply(const sen::Float32Type& type)
{
  std::ignore = type;
  EditablePrinterFunc field;
  if (bounded_)
  {
    field = boundedScalarFieldWithUnit<float32_t>(ImGuiDataType_Float, "%4.6f", unitName, quantityName, min_, max_);
  }
  else
  {
    field = scalarFieldWithUnit<float32_t>(ImGuiDataType_Float, "%4.6f", unitName, quantityName);
  }
  func_ = [field](sen::Var& var, const std::string& prefix, sen::Object* state)
  {
    ImGui::SameLine();
    return field(var, prefix, state);
  };
}

void QuantityVisitor::apply(const sen::Float64Type& type)
{
  std::ignore = type;
  if (bounded_)
  {
    func_ = boundedScalarFieldWithUnit<float64_t>(ImGuiDataType_Double, "%4.6f", unitName, quantityName, min_, max_);
  }
  else
  {
    func_ = scalarFieldWithUnit<float64_t>(ImGuiDataType_Double, "%4.6f", unitName, quantityName);
  }
}

EditablePrinterFunc QuantityVisitor::getResult() const { return func_; }
