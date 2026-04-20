// === arg_form.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_ARG_FORM_H
#define SEN_COMPONENTS_TERM_SRC_ARG_FORM_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// std
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// forward declarations
namespace sen
{
class Method;
class Type;
}  // namespace sen

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Pure-logic helpers (no FTXUI, no kernel). Used by the form UI and directly testable.
//--------------------------------------------------------------------------------------------------------------

/// True if the type is an editable scalar leaf after unwrapping aliases and optionals.
/// QuantityType is excluded (routed to a composite editor with unit picker).
[[nodiscard]] bool isScalarType(ConstTypeHandle<> type);

/// True if every argument of the method can be handled by the guided-input form.
[[nodiscard]] bool methodIsFormCompatible(const Method& method);

/// Produce a human-editable default text for a scalar Type.
[[nodiscard]] std::string defaultTextFor(ConstTypeHandle<> type);

/// Render a Var as its inline command-line representation (parseable by the command engine).
[[nodiscard]] std::string formatInlineArg(const Var& value, ConstTypeHandle<> type);

/// Assemble "objectName.methodName arg1 arg2 ..." from parsed values.
[[nodiscard]] std::string formatInlineInvocation(std::string_view objectName,
                                                 std::string_view methodName,
                                                 Span<const Arg> methodArgs,
                                                 const VarList& values);

//--------------------------------------------------------------------------------------------------------------
// Stateful form: ArgForm + ArgFormField
//--------------------------------------------------------------------------------------------------------------

/// Shape of a field in the tree.
enum class FieldKind
{
  scalar,         /// leaf with a text buffer
  structGroup,    /// named children (struct fields)
  sequenceGroup,  /// indexed children (dynamic count)
  variantGroup,   /// type-selector leaf + value subtree
  optionalGroup,  /// toggleable between empty/filled
  quantityGroup,  /// numeric value leaf + unit-selector leaf
};

/// Editor kind for a scalar leaf. The text buffer is the single source of truth.
enum class EditorKind
{
  text,          /// free-form text box
  boolean,       /// toggle between "true" and "false"
  enumeration,   /// left/right cycle through enumerator names
  variantType,   /// left/right cycle through variant alternatives
  integerSpin,   /// text with left/right to increment/decrement, clamped to range
  unitSelector,  /// left/right cycle through unit category
};

/// State for one node in the form tree. Leaves own a text buffer; composites hold children.
struct ArgFormField
{
  std::string name;
  std::string typeName;
  std::string description;
  MaybeConstTypeHandle<> type;
  FieldKind kind = FieldKind::scalar;

  // Leaf state (kind == scalar).
  std::string text;
  bool userEdited = false;
  std::string validationError;
  EditorKind editor = EditorKind::text;

  // Composite state.
  std::vector<ArgFormField> children;

  // variantGroup: selected alternative index.
  std::size_t selectedVariantIndex = 0;

  // quantityGroup integer leaves: extra clamping bounds from the quantity's min/max.
  std::optional<int64_t> quantityMinInt;
  std::optional<int64_t> quantityMaxInt;

  // optionalGroup: true when empty (no value set).
  bool optionalIsEmpty = true;

  [[nodiscard]] bool isLeaf() const noexcept { return kind == FieldKind::scalar; }
};

//--------------------------------------------------------------------------------------------------------------
// Editor helpers
//--------------------------------------------------------------------------------------------------------------

/// Toggle a bool leaf between "true" and "false".
void toggleBoolField(ArgFormField& leaf);

/// Cycle an enum leaf through its enumerator names.
void cycleEnumField(ArgFormField& leaf, bool forward);

/// Return the field's description, falling back to the type's description if empty.
[[nodiscard]] std::string_view effectiveDescription(std::string_view primary, ConstTypeHandle<> type);

/// Stateful guided-input form for a method call. Owns the field tree and navigation state.
class ArgForm final
{
  SEN_MOVE_ONLY(ArgForm)

public:
  /// Build a form for `method` on `objectName`. Returns nullopt if incompatible argument types.
  /// `prefilled` seeds the first fields; remaining get type-driven defaults.
  [[nodiscard]] static std::optional<ArgForm> build(const Method& method,
                                                    std::string_view objectName,
                                                    Span<const Var> prefilled = {});

  [[nodiscard]] const Method& method() const noexcept { return *method_; }
  [[nodiscard]] const std::string& objectName() const noexcept { return objectName_; }
  [[nodiscard]] const std::string& methodName() const noexcept { return methodName_; }
  [[nodiscard]] Span<const ArgFormField> fields() const noexcept { return fields_; }
  [[nodiscard]] std::size_t leafCount() const noexcept { return leaves_.size(); }
  [[nodiscard]] std::size_t focusedIndex() const noexcept { return focused_; }
  [[nodiscard]] const ArgFormField& focusedField() const noexcept { return *leaves_[focused_]; }

  void focusNext() noexcept;
  void focusPrev() noexcept;

  void insertText(std::string_view s);
  void backspace();
  void clearField();

  /// Toggle the focused field (bool: flip, enum: step forward).
  void toggleFocused();

  /// Cycle alternatives (bool/enum/variant). `forward` = true advances.
  void cycleFocused(bool forward);

  /// Editor kind of the focused leaf. Returns text when form is empty.
  [[nodiscard]] EditorKind focusedEditor() const noexcept;

  /// Context queries for UI key-binding hints.
  [[nodiscard]] bool focusedIsInsideSequence() const noexcept;
  [[nodiscard]] bool focusedCanAddElement() const noexcept;
  [[nodiscard]] bool focusedCanRemoveElement() const noexcept;
  [[nodiscard]] bool focusedIsInsideOptional() const noexcept;

  /// Add/remove a sequence element near the focused leaf.
  void addElementToFocusedSequence();
  void removeFocusedSequenceElement();

  /// Toggle the nearest optional ancestor between empty and filled.
  void toggleFocusedOptional();

private:
  void cycleVariantType(bool forward);

public:
  /// Validate and parse all fields. Returns parsed values or the first failing field index.
  struct SubmitError
  {
    std::size_t fieldIndex = 0;
    std::string message;
  };
  [[nodiscard]] Result<VarList, SubmitError> trySubmit();

private:
  ArgForm() = default;

  [[nodiscard]] std::optional<Var> revalidateLeaf(ArgFormField& leaf);
  void rebuildLeafCache();
  static ArgFormField buildField(std::string name, std::string description, ConstTypeHandle<> type, const Var* prefill);
  [[nodiscard]] std::optional<Var> assembleValue(ArgFormField& f, std::size_t& failIdx);

private:
  const Method* method_ = nullptr;
  std::string objectName_;
  std::string methodName_;
  std::vector<ArgFormField> fields_;
  std::vector<ArgFormField*> leaves_;
  std::size_t focused_ = 0;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_ARG_FORM_H
