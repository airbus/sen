
// === editable_printer_maker.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "editable_printer_maker.h"

// component
#include "quantity_visitor.h"
#include "util.h"
#include "value.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
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
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"

// imgui
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

// std
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] PropertyHashKey makePropertyKey(const void* object, const void* property)
{
  return {sen::hashCombine(sen::hashSeed, object, property)};
}

void drawSpinner(const char* label, const float radius, const float thickness, const ImU32& color)
{
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems)
  {
    return;
  }

  const ImGuiContext& context = *GImGui;
  const ImGuiStyle& style = context.Style;
  const ImGuiID spinnerId = window->GetID(label);

  const ImVec2 cursorPos = window->DC.CursorPos;

  const float lineHeight = ImGui::GetFrameHeight();
  const ImVec2 totalSize(radius * 2 + style.FramePadding.x, lineHeight);

  const ImRect boundingBox(cursorPos, ImVec2(cursorPos.x + totalSize.x, cursorPos.y + totalSize.y));
  ImGui::ItemSize(boundingBox, 0.0f);

  if (!ImGui::ItemAdd(boundingBox, spinnerId))
  {
    return;
  }

  window->DrawList->PathClear();

  constexpr int totalSegments = 30;
  const auto totalSegmentsFloat = sen::std_util::checkedConversion<float>(totalSegments);

  const auto timeFloat = sen::std_util::ignoredLossyConversion<float>(context.Time);
  const float sinValue = ImSin(timeFloat * 1.8f) * (totalSegmentsFloat - 5.0f);
  const int startIndex = std::abs(sen::std_util::ignoredLossyConversion<int>(sinValue));

  const auto startIndexFloat = sen::std_util::checkedConversion<float>(startIndex);
  const float minAngle = IM_PI * 2.0f * startIndexFloat / totalSegmentsFloat;
  const float maxAngle = IM_PI * 2.0f * (totalSegmentsFloat - 3.0f) / totalSegmentsFloat;

  const auto centrePos = ImVec2(cursorPos.x + style.FramePadding.x + radius, cursorPos.y + lineHeight * 0.5f);

  for (int i = 0; i < totalSegments; i++)
  {
    const float currentAngle =
      minAngle + sen::std_util::checkedConversion<float>(i) / totalSegmentsFloat * (maxAngle - minAngle);
    window->DrawList->PathLineTo(ImVec2(centrePos.x + ImCos(currentAngle + timeFloat * 8.0f) * radius,
                                        centrePos.y + ImSin(currentAngle + timeFloat * 8.0f) * radius));
  }
  window->DrawList->PathStroke(color, 0, thickness);
}

class VarInitializer final: public sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(VarInitializer)

public:
  VarInitializer() = default;
  ~VarInitializer() override = default;

  [[nodiscard]] sen::Var getResult() const noexcept { return result_; }

  void apply(const sen::StructType& type) override
  {
    std::ignore = type;
    sen::VarMap map {};
    const auto& fields = type.getFields();
    for (const auto& field: fields)
    {
      VarInitializer nested;
      field.type->accept(nested);
      map.emplace(field.name, nested.getResult());
    }
    result_ = std::move(map);
  }

  void apply(const sen::SequenceType& type) override
  {
    std::ignore = type;
    result_ = sen::VarList {};
  }

  void apply(const sen::VariantType& type) override
  {
    const auto& variantFields = type.getFields();

    SEN_ASSERT(!variantFields.empty());
    const auto& field = *variantFields.begin();
    VarInitializer nestedInit;
    field.type->accept(nestedInit);
    result_ = sen::Var(sen::KeyedVar(field.key, std::make_shared<sen::Var>(nestedInit.getResult())));
  }

  void apply(const sen::AliasType& type) override { type.getAliasedType()->accept(*this); }

  void apply(const sen::OptionalType& type) override { type.getType()->accept(*this); }

  void apply(const sen::BoolType& type) override
  {
    std::ignore = type;
    result_ = sen::Var {false};
  }
  void apply(const sen::UInt8Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<uint8_t>(0)};
  }
  void apply(const sen::Int16Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<int16_t>(0)};
  }
  void apply(const sen::UInt16Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<uint16_t>(0)};
  }
  void apply(const sen::Int32Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {0};
  }
  void apply(const sen::UInt32Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<uint32_t>(0)};
  }
  void apply(const sen::Int64Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<int64_t>(0)};
  }
  void apply(const sen::UInt64Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<uint64_t>(0)};
  }
  void apply(const sen::Float32Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<float32_t>(0)};
  }
  void apply(const sen::Float64Type& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<float64_t>(0)};
  }
  void apply(const sen::TimestampType& type) override
  {
    std::ignore = type;
    result_ = sen::Var {sen::TimeStamp {0}};
  }
  void apply(const sen::StringType& type) override
  {
    std::ignore = type;
    result_ = sen::Var {static_cast<std::string>("")};
  }
  void apply(const sen::QuantityType& type) override
  {
    VarInitializer nested;
    type.getElementType()->accept(nested);
    result_ = nested.getResult();
  }
  void apply(const sen::EnumType& type) override
  {
    const auto& enums = type.getEnums();
    if (enums.empty())
    {
      result_ = sen::Var {0};
      return;
    }
    const auto firstKey = enums.begin()->key;

    const auto& storage = type.getStorageType();
    const auto isSigned = storage.isSigned();
    switch (storage.getByteSize())
    {
      case 1:
        result_ = sen::Var {convertEnumKeyToVar<uint8_t>(firstKey)};
        break;
      case 2:
        result_ =
          sen::Var {isSigned ? convertEnumKeyToVar<int16_t>(firstKey) : convertEnumKeyToVar<uint16_t>(firstKey)};
        break;
      case 4:
        result_ =
          sen::Var {isSigned ? convertEnumKeyToVar<int32_t>(firstKey) : convertEnumKeyToVar<uint32_t>(firstKey)};
        break;
      case 8:
        result_ =
          sen::Var {isSigned ? convertEnumKeyToVar<int64_t>(firstKey) : convertEnumKeyToVar<uint64_t>(firstKey)};
        break;
      default:
        SEN_ASSERT(false && "Invalid integral");
    }
  }

private:
  sen::Var result_;

private:
  template <typename T>
  [[nodiscard]] sen::Var convertEnumKeyToVar(const uint32_t key) const
  {
    return sen::Var {sen::std_util::checkedConversion<T, sen::std_util::ReportPolicyLog>(key)};
  }
};

}  // namespace

EditablePrinterMaker::EditablePrinterMaker(sen::impl::WorkQueue* queue,
                                           PropertiesStateMap* propertiesStateMap,
                                           const bool useTrees)
  : queue_(queue), propertiesStateMap_(propertiesStateMap), useTrees_(useTrees)
{
}

[[nodiscard]] EditablePrinterFunc EditablePrinterMaker::getResult() noexcept { return func_; }

void EditablePrinterMaker::setSenVar(sen::impl::WorkQueue* queue,
                                     PropertiesStateMap* propertiesStateMap,
                                     sen::Object* owner,
                                     const sen::Property* prop,
                                     const sen::Var& var,
                                     std::function<void()> onError)
{
  if (var.isEmpty())
  {
    return;
  }
  if (owner && prop && prop->getCategory() == sen::PropertyCategory::dynamicRW)
  {
    const auto& setter = prop->getSetterMethod();

    auto callBack =
      [onError, propertiesStateMap, key = makePropertyKey(owner, prop)](const sen::MethodCallInfo&, const auto& result)
    {
      if (result.isOk())
      {
        if (propertiesStateMap)
        {
          propertiesStateMap->erase(key);
        }
      }
      else
      {
        if (onError)
        {
          onError();
        }

        std::string errorMsg = "Unknown error";
        try
        {
          std::rethrow_exception(result.getError());
        }
        catch (const std::exception& e)
        {
          errorMsg = e.what();
        }
        catch (...)
        {
          errorMsg = "Unknown failure in backend";
        }

        sen::components::explorer::getLogger()->error(errorMsg);
        if (propertiesStateMap)
        {
          (*propertiesStateMap)[key] = ErrorOnSet {errorMsg, ImGui::GetTime()};
        }
      }
    };

    owner->invokeUntyped(&setter, {var}, {queue, callBack});
  }
}

EditablePrinterFunc EditablePrinterMaker::guardField(const EditablePrinterFunc& imGuiField)
{
  EditablePrinterFunc guardedField =
    [imGuiField, guard = sen::Var {}](sen::Var& value, const std::string& prefix, sen::Object* object) mutable
  {
    bool varChanged = false;
    ImGui::PushID(&guard);
    if (imGuiField(guard, prefix, object))
    {
      value = guard;
      varChanged = true;
    }
    // if the field is not active, make sure to set the guard for the next iteration
    if (!ImGui::IsItemActive() && !varChanged)
    {
      guard = value;
    }
    ImGui::PopID();
    return varChanged;
  };
  return guardedField;
}

EditablePrinterFunc EditablePrinterMaker::getRawImGuiField() { return func_; }

EditablePrinterFunc EditablePrinterMaker::getSenPropertyField(const sen::Property* prop)
{
  EditablePrinterFunc nullfield = guardField(getRawImGuiField());
  EditablePrinterFunc f =
    [queue = queue_, propertiesStateMap = propertiesStateMap_, guardedField = std::move(nullfield), prop](
      sen::Var& var, const std::string& prefix, sen::Object* object) mutable
  {
    const auto key = makePropertyKey(object, prop);

    if (propertiesStateMap)
    {
      if (auto it = propertiesStateMap->find(key); it != propertiesStateMap->end())
      {
        if (auto* pending = std::get_if<PendingResponse>(&it->second))
        {
          double elapsedTime = ImGui::GetTime() - pending->requestTime;

          ImGui::BeginDisabled(true);

          if (elapsedTime > 0.2)
          {
            drawSpinner("##spinner", 6, 2, 0xFFD0D0D0);
            ImGui::SameLine();
          }

          sen::Var tempVar = var;
          if (guardedField)
          {
            guardedField(tempVar, prefix, object);
          }

          ImGui::EndDisabled();

          return false;
        }
      }
    }

    sen::Var oldVar = var;
    if (guardedField && guardedField(var, prefix, object))
    {
      (*propertiesStateMap)[key] = PendingResponse {ImGui::GetTime()};
      setSenVar(queue, propertiesStateMap, object, prop, var, [oldVar, &var] { var = oldVar; });
      return true;
    }

    if (propertiesStateMap)
    {
      if (auto it = propertiesStateMap->find(key); it != propertiesStateMap->end())
      {
        if (auto* error = std::get_if<ErrorOnSet>(&it->second))
        {
          if (double timeSinceError = ImGui::GetTime() - error->errorTime; timeSinceError < 5.0)
          {
            ImGui::SameLine();
          }

          sen::components::explorer::drawTooltipError(*error, "Update Failed:");

          if (error->message.empty())
          {
            propertiesStateMap->erase(it);
          }
        }
      }
    }

    return false;
  };
  return f;
}

EditablePrinterFunc EditablePrinterMaker::getGuardedField() { return guardField(getRawImGuiField()); }

template <typename ScalarType>
EditablePrinterFunc EditablePrinterMaker::scalarField(const ImGuiDataType_& type, const std::string& format)
{
  static_assert(std::is_arithmetic<ScalarType>::value, "Called scalarField with non arithmetic type!");
  EditablePrinterFunc field = [step = ScalarType {1}, stepFast = ScalarType {100}, type, format](
                                sen::Var& var, const std::string& prefix, sen::Object* object) mutable
  {
    std::ignore = prefix;
    std::ignore = object;
    ImGui::PushID(&type);
    if (var.isEmpty())
    {
      var = ScalarType {0};
    }
    ImGui::InputScalar("", type, &var, &step, &stepFast, format.c_str(), flags);
    bool varChanged = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::PopID();
    return varChanged;
  };
  return field;
}

//--------------------------------------------------------------------------------------------------------------
// Visitor Functions
//--------------------------------------------------------------------------------------------------------------

void EditablePrinterMaker::apply(const sen::EnumType& type)
{
  if (type.getStorageType().isSigned())
  {
    switch (type.getStorageType().getByteSize())
    {
      case 2:
        func_ = getTypedEnumField<int16_t>(type);
        break;
      case 4:
        func_ = getTypedEnumField<int32_t>(type);
        break;
      case 8:
        func_ = getTypedEnumField<int64_t>(type);
        break;
      default:
        func_ = nullptr;
    }
  }
  else
  {
    switch (type.getStorageType().getByteSize())
    {
      case 1:
        func_ = getTypedEnumField<uint8_t>(type);
        break;
      case 2:
        func_ = getTypedEnumField<uint16_t>(type);
        break;
      case 4:
        func_ = getTypedEnumField<uint32_t>(type);
        break;
      case 8:
        func_ = getTypedEnumField<uint64_t>(type);
        break;
      default:
        func_ = nullptr;
    }
  }
}

template <typename EnumType>
EditablePrinterFunc EditablePrinterMaker::getTypedEnumField(const sen::EnumType& type) const
{
  return [&type](sen::Var& var, const std::string& /*prefix*/, sen::Object* /*state*/)
  {
    if (var.isEmpty())
    {
      ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT(hicpp-vararg)
      return false;
    }

    const auto enums = type.getEnums();
    auto selected = type.getEnumFromKey(var.getCopyAs<EnumType>());
    auto selectedLabel = selected->name;
    bool varChanged = false;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 2.0f));
    if (ImGui::BeginCombo("", selectedLabel.c_str(), ImGuiComboFlags_WidthFitPreview))
    {
      for (const auto& enumIter: enums)
      {
        ImGui::PushID(enumIter.name.c_str());
        if (ImGui::Selectable(enumIter.name.c_str()))
        {
          var = static_cast<EnumType>(enumIter.key);
          varChanged = true;
        }
        if (enumIter.key == static_cast<uint32_t>(var.get<EnumType>()))
        {
          ImGui::SetItemDefaultFocus();
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    return varChanged;
  };
}

void EditablePrinterMaker::apply(const sen::BoolType& type)
{
  std::ignore = type;
  func_ = [](sen::Var& var, const std::string& /*prefix*/, sen::Object* /*state*/)
  {
    if (var.isEmpty())
    {
      var = false;
    }

    bool varChanged = false;

    ImGui::PushID("checkbox");
    bool b = var.get<bool>();
    if (ImGui::Checkbox("", &b))
    {
      var = b;
      varChanged = true;
    }
    ImGui::PopID();
    return varChanged;
  };
}

void EditablePrinterMaker::apply(const sen::UInt8Type& type)
{
  std::ignore = type;
  func_ = scalarField<uint8_t>(ImGuiDataType_U8, "%u");
}

void EditablePrinterMaker::apply(const sen::Int16Type& type)
{
  std::ignore = type;
  func_ = scalarField<int16_t>(ImGuiDataType_S16, "%d");
}

void EditablePrinterMaker::apply(const sen::UInt16Type& type)
{
  std::ignore = type;
  func_ = scalarField<uint16_t>(ImGuiDataType_U16, "%u");
}

void EditablePrinterMaker::apply(const sen::Int32Type& type)
{
  std::ignore = type;
  func_ = scalarField<int32_t>(ImGuiDataType_S32, "%d");
}

void EditablePrinterMaker::apply(const sen::UInt32Type& type)
{
  std::ignore = type;
  func_ = scalarField<uint32_t>(ImGuiDataType_U32, "%u");
}

void EditablePrinterMaker::apply(const sen::Int64Type& type)
{
  std::ignore = type;
  func_ = scalarField<int64_t>(ImGuiDataType_S64, "%ld");
}

void EditablePrinterMaker::apply(const sen::UInt64Type& type)
{
  std::ignore = type;
  func_ = scalarField<uint64_t>(ImGuiDataType_U64, "%lu");
}

void EditablePrinterMaker::apply(const sen::Float32Type& type)
{
  std::ignore = type;
  func_ = scalarField<float32_t>(ImGuiDataType_Float, "%4.3f");
}

void EditablePrinterMaker::apply(const sen::Float64Type& type)
{
  std::ignore = type;
  func_ = scalarField<float64_t>(ImGuiDataType_Double, "%4.3f");
}

void EditablePrinterMaker::apply(const sen::TimestampType& type)
{
  std::ignore = type;
  func_ = [](const sen::Var& value, const std::string& /*prefix*/, sen::Object* /*state*/)
  {
    if (value.isEmpty())
    {
      ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT(hicpp-vararg)
      return false;
    }

    Value::printTime(value.getCopyAs<sen::TimeStamp>());
    return false;
  };
}

void EditablePrinterMaker::apply(const sen::StringType& type)
{
  std::ignore = type;
  func_ = [guard = std::string()](sen::Var& var, const std::string& /*prefix*/, sen::Object* /*state*/) mutable
  {
    if (var.isEmpty())
    {
      var = "";
    }
    if (guard.empty() && var.holds<std::string>())
    {
      guard = var.get<std::string>();
    }
    bool valueChanged = false;
    ImGui::PushID(&guard);
    ImGui::InputText("", &guard, flags, nullptr, nullptr);
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
      valueChanged = true;
      var = guard;
    }
    ImGui::PopID();
    return valueChanged;
  };
}

void EditablePrinterMaker::apply(const sen::StructType& type)
{
  const auto fields = type.getFields();

  std::vector<EditablePrinterFunc> structFieldPrinters;
  structFieldPrinters.reserve(fields.size());
  // collect print function for each field
  for (const auto& field: fields)
  {
    // this has to include all the code needed for a new row of values
    structFieldPrinters.emplace_back(
      [&field, function = EditablePrinterFunc {}, this](
        sen::Var& var, const std::string& prefix, sen::Object* object) mutable
      {
        // get a mutable version of the map
        auto map = var.getCopyAs<sen::VarMap>();
        auto valueIter = map.find(field.name);

        if (valueIter == map.end())
        {
          // Initialize it based on the field type
          VarInitializer init;
          field.type->accept(init);
          auto initialValue = init.getResult();
          valueIter = map.emplace(field.name, initialValue).first;
          var = map;
        }

        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(field.name.c_str());
        ImGui::TableSetColumnIndex(1);
        // make sure we dont reinitialize "function", otherwise the guard
        // variable resets and we only get default values shown in the
        // interface
        if (!function)
        {
          EditablePrinterMaker visitor(queue_, propertiesStateMap_, useTrees_);
          field.type->accept(visitor);
          function = visitor.getRawImGuiField();
        }
        if (function(valueIter->second, prefix, object))
        {
          // cannot set variables of a struct individually, overwrite the
          // passed down map and return true
          var = map;
          return true;
        }
        return false;
      });
  }

  // call functions
  func_ =
    [printers = std::move(structFieldPrinters)](sen::Var& var, const std::string& prefix, sen::Object* object) mutable
  {
    bool anyChanged = false;
    ImGui::PushID("content");
    ImGui::Indent();
    {
      if (!var.holds<sen::VarMap>())
      {
        var = sen::VarMap {};
      }
      if (ImGui::BeginTable("structInternalTable", 2))
      {

        for (const auto& func: printers)
        {
          ImGui::PushID("propertyRow");
          ImGui::TableNextRow();
          // labeling done inside each print function
          if (func(var, prefix, object))
          {
            anyChanged = true;
          }
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
    }
    ImGui::Unindent();
    ImGui::PopID();
    return anyChanged;
  };
}

void EditablePrinterMaker::apply(const sen::VariantType& type)
{
  // needed to make the guardFunc be the correct type without initializing it
  EditablePrinterFunc f;

  func_ = [&type,
           selectedTypeKeyGuard = 0U,
           guardVar = sen::Var {},
           guardFunc = f,
           oldVar = sen::Var {},
           this,
           initFlag = false](sen::Var& var, const std::string& prefix, sen::Object* object) mutable
  {
    if (!var.holds<sen::KeyedVar>())
    {
      return false;
    }

    //
    // Selectable Type
    //
    bool typeChanged = false;
    bool varChanged = false;
    const auto& fields = type.getFields();
    const auto& tuple = var.get<sen::KeyedVar>();
    const auto fieldKey = std::get<0>(tuple);

    if (!initFlag)
    {
      selectedTypeKeyGuard = fieldKey;
      initFlag = true;
    }

    {
      // Generate unique label
      const std::string comboLabel = "##combo_" + std::to_string(std::hash<void*> {}(&var));
      if (ImGui::BeginCombo(comboLabel.c_str(),
                            type.getFieldFromKey(selectedTypeKeyGuard)->type->getName().data(),
                            ImGuiComboFlags_WidthFitPreview))
      {
        for (const sen::VariantField& fieldIter: fields)
        {
          ImGui::PushID(fieldIter.type->getName().data());
          if (ImGui::Selectable(fieldIter.type->getName().data(), fieldIter.key == selectedTypeKeyGuard))
          {
            selectedTypeKeyGuard = fieldIter.key;
            typeChanged = true;
          }
          if (fieldIter.key == selectedTypeKeyGuard)
          {

            ImGui::SetItemDefaultFocus();
          }
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
    }

    //
    // Value of variable
    //

    // handle change in var type
    if (typeChanged || !guardFunc)
    {
      // Handle new type selected
      auto visitor = EditablePrinterMaker(queue_, propertiesStateMap_, useTrees_);
      type.getFieldFromKey(selectedTypeKeyGuard)->type->accept(visitor);
      guardFunc = visitor.getRawImGuiField();
      // This only covers simple types. I have no idea how id handle custom
      // ones.
      if (fieldKey != selectedTypeKeyGuard)
      {
        oldVar = guardVar;
        guardVar = sen::Var {};
      }
    }

    if (fieldKey == selectedTypeKeyGuard)
    {
      // make sure our selected field type corresponds to tuple contents
      // before reading
      if (!std::get<1>(tuple)->isEmpty())
      {
        guardVar = sen::Var {*std::get<1>(tuple)};
      }
    }
    // use stable type name as ID to avoid collisions
    ImGui::PushID(type.getFieldFromKey(selectedTypeKeyGuard)->type->getName().data());
    if (guardFunc(guardVar, prefix, object))
    {
      if (guardVar.isEmpty())
      {
        // initialize default value for selected type
        auto newTypePtr = type.getFieldFromKey(selectedTypeKeyGuard)->type;
        sen::Var effectiveVar;

        VarInitializer init;
        newTypePtr->accept(init);
        effectiveVar = init.getResult();

        // store initialized value in variant
        var = sen::Var(sen::KeyedVar(selectedTypeKeyGuard, std::make_shared<sen::Var>(effectiveVar)));
      }
      else
      {
        // store user-edited value in variant
        var = sen::Var(sen::KeyedVar(selectedTypeKeyGuard, std::make_shared<sen::Var>(guardVar)));
      }
      varChanged = true;
    }
    ImGui::PopID();
    return varChanged;
  };
}

void EditablePrinterMaker::apply(const sen::SequenceType& type)
{
  std::map<ImGuiID, EditablePrinterFunc> printerMap;
  EditablePrinterFunc f;
  func_ = [&type, printers = std::move(printerMap), this](
            sen::Var& var, const std::string& prefix, sen::Object* object) mutable
  {
    if (var.isEmpty())
    {
      ImGui::TextColored(Value::getStringColor(), "<empty>");  // NOLINT(hicpp-vararg)
      return false;
    }

    if (auto theList = var.get<sen::VarList>(); !theList.empty())
    {
      ImGui::PushID(ImGui::TableGetRowIndex());
      ImGui::Indent();
      int temp = 0;
      for (auto& item: theList)
      {
        ImGui::PushID(++temp);
        if (!printers[temp])
        {
          auto v = EditablePrinterMaker(queue_, propertiesStateMap_, useTrees_);
          type.getElementType()->accept(v);
          printers[temp] = v.getRawImGuiField();
        }
        if (!item.isEmpty())
        {
          if (printers[temp](item, prefix, object))
          {
            var = theList;
            ImGui::Unindent();
            ImGui::PopID();
            ImGui::PopID();
            return true;
          }
        }
        ImGui::PopID();
      }
      ImGui::Unindent();
      ImGui::PopID();
    }
    return false;
  };
}

void EditablePrinterMaker::apply(const sen::QuantityType& type)
{
  QuantityVisitor quantityVisitor(type);
  type.getElementType()->accept(quantityVisitor);
  func_ = quantityVisitor.getResult();
}

void EditablePrinterMaker::apply(const sen::AliasType& type) { type.getAliasedType()->accept(*this); }

void EditablePrinterMaker::apply(const sen::OptionalType& type)
{
  type.getType()->accept(*this);

  auto underlyingFunc = func_;
  auto innerTypePtr = type.getType();
  func_ = [underlyingFunc, innerTypePtr](sen::Var& var, const std::string& prefix, sen::Object* object)
  {
    bool varChanged = false;
    bool hasValue = !var.isEmpty();
    bool wantValue = hasValue;

    ImGui::PushID(&var);
    if (ImGui::Checkbox("##opt_checkbox", &wantValue))
    {
      if (wantValue)
      {
        VarInitializer init;
        innerTypePtr->accept(init);
        var = init.getResult();
      }
      else
      {
        var = sen::Var {};
      }
      varChanged = true;
      hasValue = wantValue;
    }

    ImGui::SameLine();
    if (hasValue and underlyingFunc)
    {
      if (underlyingFunc(var, prefix, object))
      {
        varChanged = true;
      }
    }
    ImGui::PopID();
    return varChanged;
  };
}
