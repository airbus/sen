// === editable_printer_maker.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_EDITABLE_PRINT_MARKER_H
#define SEN_COMPONENTS_EXPLORER_SRC_EDITABLE_PRINT_MARKER_H

// component
#include "quantity_visitor.h"
#include "util.h"
#include "value.h"

// imgui
#include "imgui.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/strong_type.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// std
#include <unordered_map>
#include <variant>

SEN_STRONG_TYPE(PropertyHashKey, u32)

using EditablePrinterFunc = std::function<bool(sen::Var&, const std::string&, sen::Object*)>;

class EditablePrinterMaker final: public sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(EditablePrinterMaker)

public:
  struct PendingResponse
  {
    double requestTime;
  };

  using ErrorOnSet = sen::components::explorer::UIError;

  using PropertyAsyncState = std::variant<PendingResponse, ErrorOnSet>;

  using PropertiesStateMap = std::unordered_map<PropertyHashKey, PropertyAsyncState>;

public:
  explicit EditablePrinterMaker(sen::impl::WorkQueue* queue, PropertiesStateMap* propertiesStateMap, bool useTrees);
  ~EditablePrinterMaker() override = default;

  [[nodiscard]] EditablePrinterFunc getResult() noexcept;

public:
  ///
  /// sets a props value on sen
  ///
  /// @param[in] queue the working queue of the explorer
  /// @param[in] propertiesStateMap Map tracking the async state (loading/error) of properties
  /// @param[in] owner the owner of the prop to be changed
  /// @param[in] prop the prop which value is to be changed
  /// @param[in] var value that is supposed to be written to the prop
  /// @param[in] onError
  ///
  static void setSenVar(sen::impl::WorkQueue* queue,
                        PropertiesStateMap* propertiesStateMap,
                        sen::Object* owner,
                        const sen::Property* prop,
                        const sen::Var& var,
                        std::function<void()> onError = nullptr);
  ///
  /// Wraps a field lambda with a guard var
  ///
  /// @param[in] imGuiField an unguarded field
  ///
  static EditablePrinterFunc guardField(const EditablePrinterFunc& imGuiField);

  EditablePrinterFunc getRawImGuiField();
  EditablePrinterFunc getSenPropertyField(const sen::Property* prop);
  EditablePrinterFunc getGuardedField();

  ///
  /// Constructs an editable field for a scalar value.
  ///
  /// @tparam ScalarType an arithmetic type
  /// @param[in] type ImGuiDataType_ to inform ImGui what datatype to expect
  /// @param[in] format formatting string for the datatype
  /// @return function to draw the field for a scalar variable
  ///
  template <typename ScalarType>
  static EditablePrinterFunc scalarField(const ImGuiDataType_& type, const std::string& format);

  //--------------------------------------------------------------------------------------------------------------
  // Visitor Functions
  //--------------------------------------------------------------------------------------------------------------

  void apply(const sen::EnumType& type) override;

  template <typename EnumType>
  [[nodiscard]] EditablePrinterFunc getTypedEnumField(const sen::EnumType& type) const;

  void apply(const sen::BoolType& type) override;
  void apply(const sen::UInt8Type& type) override;
  void apply(const sen::Int16Type& type) override;
  void apply(const sen::UInt16Type& type) override;
  void apply(const sen::Int32Type& type) override;
  void apply(const sen::UInt32Type& type) override;
  void apply(const sen::Int64Type& type) override;
  void apply(const sen::UInt64Type& type) override;
  void apply(const sen::Float32Type& type) override;
  void apply(const sen::Float64Type& type) override;
  void apply(const sen::TimestampType& type) override;
  void apply(const sen::StringType& type) override;
  void apply(const sen::StructType& type) override;
  void apply(const sen::VariantType& type) override;
  void apply(const sen::SequenceType& type) override;
  void apply(const sen::QuantityType& type) override;
  void apply(const sen::AliasType& type) override;
  void apply(const sen::OptionalType& type) override;

private:
  EditablePrinterFunc func_ = nullptr;
  sen::impl::WorkQueue* queue_;
  PropertiesStateMap* propertiesStateMap_;
  bool useTrees_ = true;
  static constexpr ImGuiInputTextFlags flags = ImGuiInputTextFlags_None |          // NOLINT(hicpp-signed-bitwise)
                                               ImGuiInputTextFlags_AutoSelectAll;  // NOLINT(hicpp-signed-bitwise)
};

SEN_STRONG_TYPE_HASHABLE(PropertyHashKey)

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline EditablePrinterFunc getOrCreateEditablePrinter(sen::impl::WorkQueue* queue,
                                                      EditablePrinterMaker::PropertiesStateMap* propertiesStateMap,
                                                      const sen::Type& type,
                                                      const sen::Property* prop = nullptr,
                                                      const bool useTreesForCompounds = true)
{
  EditablePrinterMaker visitor(queue, propertiesStateMap, useTreesForCompounds);
  type.accept(visitor);

  return prop ? visitor.getSenPropertyField(prop) : visitor.getResult();
}

#endif  // SEN_COMPONENTS_EXPLORER_SRC_EDITABLE_PRINT_MARKER_H
