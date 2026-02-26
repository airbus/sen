// === quantity_visitor.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_QUANTITY_VISITOR_H
#define SEN_COMPONENTS_EXPLORER_SRC_QUANTITY_VISITOR_H

// imgui
#include "imgui.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/obj/object.h"

// std
#include <optional>
#include <string>

class ObjectState;
using EditablePrinterFunc = std::function<bool(sen::Var&, const std::string&, sen::Object*)>;

class QuantityVisitor: public sen::TypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(QuantityVisitor)

  QuantityVisitor(std::string unitName,
                  std::string quantityName,
                  bool bounded,
                  std::optional<float64_t> lowerBound,
                  std::optional<float64_t> upperBound);

  explicit QuantityVisitor(const sen::QuantityType& quantityType);

  ~QuantityVisitor() override = default;

public:
  [[nodiscard]] EditablePrinterFunc getResult() const;

private:
  const std::string unitName;
  const std::string quantityName;
  bool bounded_;
  std::optional<float64_t> min_;
  std::optional<float64_t> max_;

  ///
  /// @func_ A function constructing an ImGui Input Field to edit a sen
  /// property. Returns true if the property was changed, or (for structs) is
  /// supposed to be changed. Takes in ImGuiInputTextFlags and a sen var
  /// reference.
  ///
  EditablePrinterFunc func_;

  ///
  /// Constructs an editable field for a scalar value.
  ///
  /// @tparam ScalarType an arithmetic type
  /// @param[in] var current value of the property
  /// @param[in] type ImGuiDataType_ to inform ImGui what datatype to expect
  /// @param[in] format formatting string for the datatype
  /// @return function to draw the field for a scalar variable
  ///
  template <typename ScalarType>
  static EditablePrinterFunc scalarFieldWithUnit(const ImGuiDataType_& type,
                                                 const std::string& format,
                                                 const std::string& unit,
                                                 const std::string& quantityName);

  //
  /// Constructs an editable field for a scalar value appending the respective
  /// unit.
  ///
  /// @tparam ScalarType an arithmetic type
  /// @param[in] var current value of the property
  /// @param[in] unit description of unit
  /// @param[in] type ImGuiDataType_ to inform ImGui what datatype to expect
  /// @param[in] format formatting string for the datatype
  /// @return function to draw the field for a scalar variable
  ///
  template <typename ScalarType>
  static EditablePrinterFunc boundedScalarFieldWithUnit(const ImGuiDataType_& type,
                                                        const std::string& format,
                                                        const std::string& unit,
                                                        const std::string& quantityName,
                                                        std::optional<float64_t>& min,
                                                        std::optional<float64_t>& max);

  /// Visitor functions
  void apply(const sen::IntegralType& type) override;

  void apply(const sen::RealType& type) override;

  void apply(const sen::UInt8Type& type) override;

  void apply(const sen::UInt16Type& type) override;

  void apply(const sen::UInt32Type& type) override;
  void apply(const sen::UInt64Type& type) override;
  void apply(const sen::Int16Type& type) override;
  void apply(const sen::Int32Type& type) override;
  void apply(const sen::Int64Type& type) override;
  void apply(const sen::Float32Type& type) override;
  void apply(const sen::Float64Type& type) override;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline ImGuiInputTextFlags getFlags()
{

  constexpr ImGuiInputTextFlags flags = ImGuiInputTextFlags_None |          // NOLINT(hicpp-signed-bitwise)
                                        ImGuiInputTextFlags_AutoSelectAll;  // NOLINT(hicpp-signed-bitwise)
  return flags;
}

template <typename ScalarType>
EditablePrinterFunc QuantityVisitor::scalarFieldWithUnit(const ImGuiDataType_& type,
                                                         const std::string& format,
                                                         const std::string& unit,
                                                         const std::string& quantityName)
{
  static_assert(std::is_arithmetic<ScalarType>::value, "Called scalarField with non arithmetic type!");
  EditablePrinterFunc field = [step = ScalarType {1}, stepFast = ScalarType {100}, type, format, unit, quantityName](
                                sen::Var& var, const std::string& /*prefix*/, sen::Object* /*state*/) mutable
  {
    ImGui::PushID(&type);
    if (var.isEmpty())
    {
      var = ScalarType {0};
    }
    std::string label = quantityName;
    label += " [" + unit + "]";
    ImGui::TextUnformatted(label.c_str());
    ImGui::InputScalar("", type, &var, &step, &stepFast, format.c_str(), getFlags());
    bool varChanged = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::PopID();
    return varChanged;
  };
  return field;
}

template <typename ScalarType>
EditablePrinterFunc QuantityVisitor::boundedScalarFieldWithUnit(const ImGuiDataType_& type,
                                                                const std::string& format,
                                                                const std::string& unit,
                                                                const std::string& quantityName,
                                                                std::optional<float64_t>& min,
                                                                std::optional<float64_t>& max)
{
  static_assert(std::is_arithmetic<ScalarType>::value, "Called scalarField with non arithmetic type!");
  EditablePrinterFunc field =
    [step = ScalarType {1}, stepFast = ScalarType {100}, type, format, unit, quantityName, min, max](
      sen::Var& var, const std::string& /*prefix*/, sen::Object* /*state*/) mutable
  {
    ImGui::PushID(&type);
    if (var.isEmpty())
    {
      var = ScalarType {0};
    }
    std::string label = quantityName;
    label += " [" + unit + "]";
    ImGui::TextUnformatted(label.c_str());
    ImGui::InputScalar("", type, &var, &step, &stepFast, format.c_str(), getFlags());
    bool varChanged = ImGui::IsItemDeactivatedAfterEdit();
    // limit by bounds
    if (varChanged)
    {
      ScalarType actualMin =
        min.has_value() ? static_cast<ScalarType>(min.value()) : std::numeric_limits<ScalarType>::min();
      ScalarType actualMax =
        max.has_value() ? static_cast<ScalarType>(max.value()) : std::numeric_limits<ScalarType>::max();

      if (var.get<ScalarType>() < actualMin)
      {
        var = actualMin;
      }
      else if (var.get<ScalarType>() > actualMax)
      {
        var = actualMax;
      }
    }

    ImGui::PopID();
    return varChanged;
  };
  return field;
}

#endif  // SEN_COMPONENTS_EXPLORER_SRC_QUANTITY_VISITOR_H
