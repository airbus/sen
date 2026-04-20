// === arg_form.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "arg_form.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/duration.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
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

/// Sentinel returned by leaf-index lookups when no match is found.
constexpr std::size_t noIndex = std::numeric_limits<std::size_t>::max();

/// Unwrap Alias and Optional to reach the underlying leaf type for scalar classification.
ConstTypeHandle<> unwrapScalar(ConstTypeHandle<> type)
{
  if (type->isAliasType())
  {
    return unwrapScalar(type->asAliasType()->getAliasedType());
  }
  if (type->isOptionalType())
  {
    return unwrapScalar(type->asOptionalType()->getType());
  }
  return type;
}

/// Concrete scalar leaves editable inline. QuantityType excluded (buildField dispatches it separately).
bool isConcreteScalar(ConstTypeHandle<> t)
{
  return t->isBoolType() || t->isInt16Type() || t->isInt32Type() || t->isInt64Type() || t->isUInt8Type() ||
         t->isUInt16Type() || t->isUInt32Type() || t->isUInt64Type() || t->isFloat32Type() || t->isFloat64Type() ||
         t->isStringType() || t->isEnumType() || t->isDurationType() || t->isTimestampType();
}

/// JSON-escape and double-quote a string for the command engine's parser.
std::string quoteString(std::string_view s)
{
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  for (char c: s)
  {
    switch (c)
    {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20)
        {
          // Control chars -> \u00XX for JSON round-trippability.
          std::ostringstream esc;
          esc << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
          out += esc.str();
        }
        else
        {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
  return out;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Type predicates
//--------------------------------------------------------------------------------------------------------------

bool isScalarType(ConstTypeHandle<> type) { return isConcreteScalar(unwrapScalar(type)); }

namespace
{

/// Recursive check: scalars always pass; composites pass iff all children do.
bool isFormCompatible(ConstTypeHandle<> type)
{
  if (isScalarType(type))
  {
    return true;
  }

  // Unwrap alias transparently.
  ConstTypeHandle<> t = type;
  for (auto* a = t->asAliasType(); a != nullptr; a = t->asAliasType())
  {
    t = a->getAliasedType();
  }

  if (auto* s = t->asStructType(); s != nullptr)
  {
    for (const auto& field: s->getAllFields())
    {
      if (!isFormCompatible(field.type))
      {
        return false;
      }
    }
    return true;
  }

  if (auto* s = t->asSequenceType(); s != nullptr)
  {
    // Sequence: element type must be form-compatible.
    return isFormCompatible(s->getElementType());
  }

  if (auto* v = t->asVariantType(); v != nullptr)
  {
    // Variant: every alternative must be form-compatible.
    for (const auto& field: v->getFields())
    {
      if (!isFormCompatible(field.type))
      {
        return false;
      }
    }
    return true;
  }

  if (auto* o = t->asOptionalType(); o != nullptr)
  {
    // Optional: inner type must be form-compatible.
    return isFormCompatible(o->getType());
  }

  if (auto* q = t->asQuantityType(); q != nullptr)
  {
    // All quantities with numeric storage are form-compatible.
    (void)q;
    return true;
  }

  return false;
}

}  // namespace

bool methodIsFormCompatible(const Method& method)
{
  for (const auto& arg: method.getArgs())
  {
    if (!isFormCompatible(arg.type))
    {
      return false;
    }
  }
  return true;
}

//--------------------------------------------------------------------------------------------------------------
// Defaults
//--------------------------------------------------------------------------------------------------------------

std::string defaultTextFor(ConstTypeHandle<> type)
{
  const auto& leaf = unwrapScalar(type);
  if (leaf->isBoolType())
  {
    return "false";
  }
  if (leaf->isStringType())
  {
    return {};  // empty; UI shows a placeholder
  }
  if (auto* e = leaf->asEnumType(); e != nullptr)
  {
    if (!e->getEnums().empty())
    {
      return std::string(e->getEnums()[0].name);
    }
    return {};
  }
  if (leaf->isTimestampType())
  {
    return {};  // no obvious default; user must provide
  }
  // All other scalars (int/float/Duration/Quantity) default to 0.
  return "0";
}

//--------------------------------------------------------------------------------------------------------------
// Inline formatting
//--------------------------------------------------------------------------------------------------------------

std::string formatInlineArg(const Var& value, ConstTypeHandle<> type)
{
  // Composite types before unwrapping: per-field formatting for type-aware rendering.
  ConstTypeHandle<> ct = type;
  for (auto* a = ct->asAliasType(); a != nullptr; a = ct->asAliasType())
  {
    ct = a->getAliasedType();
  }

  // Optional
  if (auto* optType = ct->asOptionalType(); optType != nullptr)
  {
    if (value.holds<std::monostate>())
    {
      return "null";
    }
    return formatInlineArg(value, optType->getType());
  }
  if (auto* structType = ct->asStructType(); structType != nullptr)
  {
    const auto* map = value.getIf<VarMap>();
    if (map == nullptr)
    {
      return "{}";
    }
    std::string out = "{";
    bool first = true;
    for (const auto& field: structType->getAllFields())
    {
      auto it = map->find(field.name);
      if (it == map->end())
      {
        continue;
      }
      if (!first)
      {
        out += ", ";
      }
      first = false;
      out += quoteString(field.name);
      out += ": ";
      out += formatInlineArg(it->second, field.type);
    }
    out += "}";
    return out;
  }

  if (auto* seqType = ct->asSequenceType(); seqType != nullptr)
  {
    const auto* list = value.getIf<VarList>();
    if (list == nullptr)
    {
      return "[]";
    }
    const auto elemType = seqType->getElementType();
    std::string out = "[";
    for (std::size_t i = 0; i < list->size(); ++i)
    {
      if (i > 0)
      {
        out += ", ";
      }
      out += formatInlineArg((*list)[i], elemType);
    }
    out += "]";
    return out;
  }

  if (auto* variantType = ct->asVariantType(); variantType != nullptr)
  {
    // Variant may be KeyedVar (adapted) or raw VarMap; render both uniformly.
    std::string typeName;
    const Var* inner = nullptr;

    if (auto* kv = value.getIf<KeyedVar>(); kv != nullptr)
    {
      auto [idx, innerPtr] = *kv;
      const auto fields = variantType->getFields();
      if (idx < fields.size())
      {
        typeName = std::string(fields[idx].type->getName());
      }
      if (innerPtr)
      {
        inner = innerPtr.get();
      }
    }
    else if (auto* vm = value.getIf<VarMap>(); vm != nullptr)
    {
      if (auto it = vm->find("type"); it != vm->end())
      {
        if (auto* s = it->second.getIf<std::string>(); s != nullptr)
        {
          typeName = *s;
        }
      }
      if (auto it = vm->find("value"); it != vm->end())
      {
        inner = &it->second;
      }
    }

    if (typeName.empty())
    {
      return "null";
    }

    // Find the inner type for correct value formatting.
    MaybeConstTypeHandle<> innerType = {};
    for (const auto& field: variantType->getFields())
    {
      if (field.type->getName() == typeName)
      {
        innerType = field.type;
        break;
      }
    }

    std::string out = "{";
    out += quoteString("type");
    out += ": ";
    out += quoteString(typeName);
    if (inner != nullptr && innerType.has_value())
    {
      out += ", ";
      out += quoteString("value");
      out += ": ";
      out += formatInlineArg(*inner, innerType.value());
    }
    out += "}";
    return out;
  }

  const auto leaf = unwrapScalar(type);

  if (leaf->isStringType())
  {
    if (auto* s = value.getIf<std::string>(); s != nullptr)
    {
      return quoteString(*s);
    }
    return "\"\"";
  }

  if (leaf->isBoolType())
  {
    if (auto* b = value.getIf<bool>(); b != nullptr)
    {
      return *b ? "true" : "false";
    }
    return "false";
  }

  if (auto* e = leaf->asEnumType(); e != nullptr)
  {
    // Enum values may be the name (string) or integer key; always render the name.
    if (auto* s = value.getIf<std::string>(); s != nullptr)
    {
      return quoteString(*s);
    }
    try
    {
      if (const auto* entry = e->getEnumFromKey(value.getCopyAs<uint32_t>()); entry != nullptr)
      {
        return quoteString(entry->name);
      }
    }
    catch (...)
    {
    }
    return "\"\"";
  }

  if (leaf->isDurationType())
  {
    // Emit as "N unit" string so the round-trip goes through Unit::fromString at the right scale.
    // Without the unit suffix, bare numerics would be read as nanoseconds.
    if (auto* s = value.getIf<std::string>(); s != nullptr)
    {
      return quoteString(*s);
    }
    if (auto* d = value.getIf<Duration>(); d != nullptr)
    {
      std::ostringstream oss;
      oss << d->toSeconds();
      return quoteString(oss.str() + " s");
    }
    try
    {
      auto ns = value.getCopyAs<int64_t>();
      return quoteString(std::to_string(ns) + " ns");
    }
    catch (...)
    {
      return quoteString("0 s");
    }
  }

  // Numeric: fall through to JSON serialiser.
  return toJson(value);
}

std::string_view effectiveDescription(std::string_view primary, ConstTypeHandle<> type)
{
  return !primary.empty() ? primary : type->getDescription();
}

std::string formatInlineInvocation(std::string_view objectName,
                                   std::string_view methodName,
                                   Span<const Arg> methodArgs,
                                   const VarList& values)
{
  std::string out;
  out += objectName;
  out += '.';
  out += methodName;

  const auto count = std::min(methodArgs.size(), values.size());
  for (std::size_t i = 0; i < count; ++i)
  {
    out.push_back(' ');
    out += formatInlineArg(values[i], methodArgs[i].type);
  }

  return out;
}

//--------------------------------------------------------------------------------------------------------------
// ArgForm
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Parse a field's text into a Var, mirroring the command engine's inline invocation rules.
Result<Var, std::string> parseFieldText(std::string_view text, const Type& type)
{
  const Type* t = &type;
  for (auto* a = t->asAliasType(); a != nullptr; a = t->asAliasType())
  {
    t = a->getAliasedType().type();
  }
  const Type& leaf = *t;

  // String: verbatim.
  if (leaf.isStringType())
  {
    Var v {std::string(text)};
    if (auto r = impl::adaptVariant(type, v); r.isError())
    {
      return Err(r.getError());
    }
    return Ok(std::move(v));
  }

  // Non-string: empty text is always an error.
  if (text.empty())
  {
    return Err(std::string("value required"));
  }

  // Enum: accept bare name directly; quoted text falls through to JSON path.
  if (leaf.isEnumType() && text.front() != '"')
  {
    Var v {std::string(text)};
    if (auto r = impl::adaptVariant(type, v); r.isError())
    {
      return Err(r.getError());
    }
    return Ok(std::move(v));
  }

  // Duration: bare numbers default to seconds; explicit suffixes ("500ms") pass through.
  if (leaf.isDurationType() && text.front() != '"')
  {
    auto hasUnitSuffix = [&]
    {
      // Non-numeric chars mean the user already typed a unit suffix.
      return text.find_first_not_of("+-0123456789.eE ") != std::string_view::npos;
    }();
    std::string asSeconds = std::string(text) + (hasUnitSuffix ? "" : " s");
    Var v {std::move(asSeconds)};
    if (auto r = impl::adaptVariant(type, v); r.isError())
    {
      return Err(r.getError());
    }
    return Ok(std::move(v));
  }

  // Everything else: parse as JSON literal.
  try
  {
    auto doc = std::string("{ \"v\": ") + std::string(text) + " }";
    auto parsed = fromJson(doc).get<VarMap>();
    auto itr = parsed.find("v");
    if (itr == parsed.end())
    {
      return Err(std::string("parse error"));
    }
    Var value = std::move(itr->second);
    if (auto r = impl::adaptVariant(type, value); r.isError())
    {
      return Err(r.getError());
    }
    return Ok(std::move(value));
  }
  catch (const std::exception&)
  {
    // Replace cryptic JSON parser errors with actionable type-specific hints.
    const auto& leaf = *t;
    if (leaf.isEnumType())
    {
      return Err(std::string("invalid enumerator name"));
    }
    if (leaf.asIntegralType() != nullptr)
    {
      return Err(std::string("expected an integer"));
    }
    if (leaf.isFloat32Type() || leaf.isFloat64Type())
    {
      return Err(std::string("expected a number"));
    }
    return Err(std::string("invalid value"));
  }
}

/// Truncate one UTF-8 code point from the end of `text`.
void truncateOneCodePoint(std::string& text)
{
  if (text.empty())
  {
    return;
  }
  // Skip continuation bytes (10xxxxxx), then drop the lead byte.
  std::size_t i = text.size();
  while (i > 0 && (static_cast<unsigned char>(text[i - 1]) & 0xC0) == 0x80)
  {
    --i;
  }
  if (i > 0)
  {
    --i;
  }
  text.resize(i);
}

}  // namespace

namespace
{

/// Peel alias/optional/quantity wrappers to reach the leaf type that drives editor classification.
MaybeConstTypeHandle<> unwrapToLeafType(MaybeConstTypeHandle<> t)
{
  if (!t.has_value())
  {
    return {};
  }

  bool peeled = true;
  while (peeled)
  {
    peeled = false;
    if (auto* a = t.value()->asAliasType(); a != nullptr)
    {
      t = a->getAliasedType();
      peeled = true;
    }
    if (auto* o = t.value()->asOptionalType(); o != nullptr)
    {
      t = o->getType();
      peeled = true;
    }
    if (auto* q = t.value()->asQuantityType(); q != nullptr)
    {
      // Duration/TimeStamp use a string-with-unit path; peeling would mis-classify them as integerSpin.
      if (!t.value()->isDurationType() && !t.value()->isTimestampType())
      {
        t = q->getElementType();
        peeled = true;
      }
    }
  }
  return t;
}

/// Returns the NumericType if the leaf is an integer (any width/signedness), else nullptr.
MaybeConstTypeHandle<NumericType> asIntegerType(MaybeConstTypeHandle<> t)
{
  const auto leaf = unwrapToLeafType(t);
  if (!leaf.has_value() || !leaf.value()->isIntegralType())
  {
    return {};
  }

  return dynamicTypeHandleCast<const NumericType>(t.value());
}

/// True for float, Duration, Timestamp, Quantity (types sharing the float-like input grammar).
bool isFloatLikeType(MaybeConstTypeHandle<> t)
{
  const auto leaf = unwrapToLeafType(t);
  if (!leaf.has_value())
  {
    return false;
  }

  return leaf.value()->isFloat32Type() || leaf.value()->isFloat64Type() || leaf.value()->isDurationType() ||
         leaf.value()->isTimestampType() || (leaf.value()->asRealType() != nullptr) ||
         (leaf.value()->asQuantityType() != nullptr);
}

/// Per-keystroke filter: returns true if `c` is plausible for a value of `type`.
bool isAcceptableChar(char c, MaybeConstTypeHandle<> type, std::string_view current)
{
  if (!type.has_value())
  {
    return true;
  }

  const auto leaf = unwrapToLeafType(type);
  if (!leaf.has_value() || leaf.value()->isStringType())
  {
    return true;
  }

  // Duration/TimeStamp accept unit suffixes ("500 ms", "3h"), so allow all chars.
  if (leaf.value()->isDurationType() || leaf.value()->isTimestampType())
  {
    return true;
  }

  if (const auto* i = leaf.value()->asIntegralType(); i != nullptr)
  {
    if (std::isdigit(static_cast<unsigned char>(c)) != 0)
    {
      return true;
    }
    // Leading minus for signed types only.
    if (c == '-' && i->isSigned() && current.empty())
    {
      return true;
    }
    return false;
  }

  if (isFloatLikeType(leaf))
  {
    if (std::isdigit(static_cast<unsigned char>(c)) != 0)
    {
      return true;
    }
    // Float grammar: sign, decimal point, exponent. Loose order; adaptVariant validates fully.
    if (c == '-' || c == '+')
    {
      if (current.empty())
      {
        return true;
      }
      auto last = current.back();
      return (last == 'e' || last == 'E');
    }
    if (c == '.')
    {
      return current.find('.') == std::string_view::npos;
    }
    if (c == 'e' || c == 'E')
    {
      // Exponent: once, after at least one digit.
      if (current.find('e') != std::string_view::npos || current.find('E') != std::string_view::npos)
      {
        return false;
      }
      for (auto ch: current)
      {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0)
        {
          return true;
        }
      }
      return false;
    }
    return false;
  }

  return true;
}

/// Filter an inserted string against the leaf's type rules; drop invalid chars.
std::string filterAcceptableChars(std::string_view incoming, MaybeConstTypeHandle<> type, std::string_view current)
{
  std::string out;
  out.reserve(incoming.size());
  std::string running(current);
  for (char c: incoming)
  {
    if (isAcceptableChar(c, type, running))
    {
      out.push_back(c);
      running.push_back(c);
    }
  }
  return out;
}

/// Clamp a 64-bit integer to the target integral type's representable range.
int64_t clampToIntegerRange(int64_t value, const NumericType& nt)
{
  const auto bytes = nt.getByteSize();
  const bool s = nt.isSigned();
  int64_t lo = std::numeric_limits<int64_t>::min();
  int64_t hi = std::numeric_limits<int64_t>::max();
  if (s)
  {
    switch (bytes)
    {
      case 2:
        lo = std::numeric_limits<int16_t>::min();
        hi = std::numeric_limits<int16_t>::max();
        break;
      case 4:
        lo = std::numeric_limits<int32_t>::min();
        hi = std::numeric_limits<int32_t>::max();
        break;
      case 8: /* int64, no tighter bound */
        break;
      default:
        break;
    }
  }
  else
  {
    lo = 0;
    switch (bytes)
    {
      case 1:
        hi = std::numeric_limits<uint8_t>::max();
        break;
      case 2:
        hi = std::numeric_limits<uint16_t>::max();
        break;
      case 4:
        hi = std::numeric_limits<uint32_t>::max();
        break;
      case 8: /* uint64: high capped at int64 max, which is our working precision */
        break;
      default:
        break;
    }
  }
  return std::max(lo, std::min(value, hi));
}

/// Parse text as int64; returns 0 on failure so spins start from a clean slate.
int64_t parseIntegerLeaf(std::string_view text)
{
  if (text.empty())
  {
    return 0;
  }
  try
  {
    return std::stoll(std::string(text));
  }
  catch (...)
  {
    return 0;
  }
}

/// Spin an integer leaf by `delta`, clamping to type range and quantity min/max if declared.
void spinIntegerLeaf(ArgFormField& leaf, int64_t delta)
{
  const auto numeric = asIntegerType(leaf.type);
  if (!numeric.has_value())
  {
    return;
  }
  const auto current = parseIntegerLeaf(leaf.text);
  auto next = clampToIntegerRange(current + delta, *numeric.value());
  if (leaf.quantityMinInt.has_value())
  {
    next = std::max(next, *leaf.quantityMinInt);
  }
  if (leaf.quantityMaxInt.has_value())
  {
    next = std::min(next, *leaf.quantityMaxInt);
  }
  leaf.text = std::to_string(next);
  leaf.userEdited = true;
  leaf.validationError.clear();
}

/// Map a leaf type to its editor kind; unrecognised types fall back to text.
EditorKind classifyEditor(ConstTypeHandle<> type)
{
  for (auto* a = type->asAliasType(); a != nullptr; a = type->asAliasType())
  {
    type = a->getAliasedType();
  }
  if (auto* o = type->asOptionalType(); o != nullptr)
  {
    return classifyEditor(o->getType());
  }
  if (type->isBoolType())
  {
    return EditorKind::boolean;
  }
  if (type->isEnumType())
  {
    return EditorKind::enumeration;
  }
  if (asIntegerType(type).has_value())
  {
    return EditorKind::integerSpin;
  }
  return EditorKind::text;
}

}  // namespace

ArgFormField ArgForm::buildField(std::string name, std::string description, ConstTypeHandle<> type, const Var* prefill)
{
  ArgFormField f;
  f.name = std::move(name);
  f.typeName = type->getName();
  f.description = std::move(description);
  f.type = type;

  // Unwrap alias.
  ConstTypeHandle<> t = type;
  for (auto* a = t->asAliasType(); a != nullptr; a = t->asAliasType())
  {
    t = a->getAliasedType();
  }

  if (auto* structType = t->asStructType(); structType != nullptr)
  {
    f.kind = FieldKind::structGroup;
    // Reserve to keep child pointers stable.
    const auto allFields = structType->getAllFields();
    f.children.reserve(allFields.size());

    const VarMap* prefillMap = (prefill != nullptr) ? prefill->getIf<VarMap>() : nullptr;

    for (const auto& sf: allFields)
    {
      const Var* childPrefill = nullptr;
      if (prefillMap != nullptr)
      {
        auto it = prefillMap->find(sf.name);
        if (it != prefillMap->end())
        {
          childPrefill = &it->second;
        }
      }
      f.children.push_back(buildField(sf.name, sf.description, sf.type, childPrefill));
    }
    return f;
  }

  if (auto* seqType = t->asSequenceType(); seqType != nullptr)
  {
    f.kind = FieldKind::sequenceGroup;
    // Sequence: fixed-size arrays get N default elements; dynamic sequences start empty or from prefill.
    auto elemType = seqType->getElementType();

    const VarList* prefillList = (prefill != nullptr) ? prefill->getIf<VarList>() : nullptr;
    std::size_t initialCount = (prefillList != nullptr) ? prefillList->size() : 0U;
    if (prefillList == nullptr && seqType->hasFixedSize())
    {
      // Fixed-size array: must start with exactly N elements.
      initialCount = seqType->getMaxSize().value_or(0U);
    }
    f.children.reserve(initialCount);

    for (std::size_t i = 0; i < initialCount; ++i)
    {
      const Var* elemPrefill = (prefillList != nullptr && i < prefillList->size()) ? &(*prefillList)[i] : nullptr;
      f.children.push_back(buildField("[" + std::to_string(i) + "]", "", elemType, elemPrefill));
    }
    return f;
  }

  if (auto* optType = t->asOptionalType(); optType != nullptr)
  {
    // All optionals route through optionalGroup for explicit "(none)" state and Ctrl+O toggle.
    f.kind = FieldKind::optionalGroup;
    const bool hasValue = (prefill != nullptr) && !prefill->holds<std::monostate>();
    f.optionalIsEmpty = !hasValue;

    f.children.reserve(1);
    if (hasValue)
    {
      f.children.push_back(buildField("value", "", optType->getType(), prefill));
    }
    return f;
  }

  if (t->isQuantityType())
  {
    auto quantityType = dynamicTypeHandleCast<const QuantityType>(t).value();

    // Duration/TimeStamp need string-with-unit dispatch, not the generic f64 quantityGroup path.
    // A bare f64 would make getCopyAs<Duration>() throw at call time.
    if (!t->isDurationType() && !t->isTimestampType())
    {
      const auto canonicalUnit = quantityType->getUnit();
      const auto elementType = quantityType->getElementType();

      // Quantity range hints for spinIntegerLeaf clamping. Extreme float bounds (e.g. FLT_MAX)
      // are clamped silently to int64 limits via ReportPolicyIgnore.
      std::optional<int64_t> quantityMinInt;
      std::optional<int64_t> quantityMaxInt;
      if (auto mn = quantityType->getMinValue(); mn.has_value())
      {
        quantityMinInt = checkedConversion<int64_t, sen::std_util::ReportPolicyIgnore>(*mn);
      }
      if (auto mx = quantityType->getMaxValue(); mx.has_value())
      {
        quantityMaxInt = checkedConversion<int64_t, sen::std_util::ReportPolicyIgnore>(*mx);
      }

      if (!canonicalUnit.has_value())
      {
        // Unit-less quantity -> plain scalar leaf with the storage element type.
        f.kind = FieldKind::scalar;
        f.editor = classifyEditor(elementType);
        f.quantityMinInt = quantityMinInt;
        f.quantityMaxInt = quantityMaxInt;
        if (prefill != nullptr)
        {
          f.text = formatInlineArg(*prefill, type);
          f.userEdited = true;
        }
        else
        {
          f.text = defaultTextFor(elementType);
          f.userEdited = false;
        }
        return f;
      }

      // With-unit quantity -> value leaf + unit-selector leaf.
      f.kind = FieldKind::quantityGroup;
      f.children.reserve(2);

      ArgFormField valueLeaf;
      valueLeaf.name = "value";
      valueLeaf.typeName = std::string(elementType->getName());
      valueLeaf.type = elementType;
      valueLeaf.kind = FieldKind::scalar;
      valueLeaf.editor = classifyEditor(elementType);
      valueLeaf.quantityMinInt = quantityMinInt;
      valueLeaf.quantityMaxInt = quantityMaxInt;
      if (prefill != nullptr)
      {
        valueLeaf.text = formatInlineArg(*prefill, elementType);
        valueLeaf.userEdited = true;
      }
      else
      {
        valueLeaf.text = defaultTextFor(elementType);
        valueLeaf.userEdited = false;
      }
      f.children.push_back(std::move(valueLeaf));

      ArgFormField unitLeaf;
      unitLeaf.name = "unit";
      unitLeaf.typeName = std::string(Unit::getCategoryString((*canonicalUnit)->getCategory()));
      // Unit leaf carries the QuantityType so cycleUnitField can access the category.
      unitLeaf.type = quantityType;
      unitLeaf.kind = FieldKind::scalar;
      unitLeaf.editor = EditorKind::unitSelector;
      unitLeaf.text = std::string((*canonicalUnit)->getAbbreviation());
      unitLeaf.userEdited = true;
      f.children.push_back(std::move(unitLeaf));

      return f;
    }  // Duration/TimeStamp fall through to scalar branch below.
  }

  if (t->isVariantType())
  {
    auto variantType = dynamicTypeHandleCast<const VariantType>(t).value();

    f.kind = FieldKind::variantGroup;
    const auto variantFields = variantType->getFields();
    if (variantFields.empty())
    {
      return f;  // Pathological: no alternatives.
    }

    // Determine starting alternative from prefill (KeyedVar or raw VarMap).
    std::size_t selectedIdx = 0;
    const Var* innerPrefill = nullptr;
    if (prefill != nullptr)
    {
      if (auto* kv = prefill->getIf<KeyedVar>(); kv != nullptr)
      {
        auto [idx, innerPtr] = *kv;
        if (idx < variantFields.size())
        {
          selectedIdx = idx;
        }
        if (innerPtr)
        {
          innerPrefill = innerPtr.get();
        }
      }
      else if (auto* vm = prefill->getIf<VarMap>(); vm != nullptr)
      {
        if (auto it = vm->find("type"); it != vm->end())
        {
          if (auto* s = it->second.getIf<std::string>(); s != nullptr)
          {
            for (std::size_t i = 0; i < variantFields.size(); ++i)
            {
              if (variantFields[i].type->getName() == *s)
              {
                selectedIdx = i;
                break;
              }
            }
          }
        }
        if (auto it = vm->find("value"); it != vm->end())
        {
          innerPrefill = &it->second;
        }
      }
    }

    f.children.reserve(2);

    // Child[0]: type-selector leaf.
    ArgFormField typeLeaf;
    typeLeaf.name = "type";
    typeLeaf.typeName = std::string(variantType->getName());
    typeLeaf.description = "variant type selector";
    typeLeaf.type = variantType;
    typeLeaf.kind = FieldKind::scalar;
    typeLeaf.editor = EditorKind::variantType;
    typeLeaf.text = std::string(variantFields[selectedIdx].type->getName());
    typeLeaf.userEdited = (prefill != nullptr);
    f.children.push_back(std::move(typeLeaf));

    // Child[1]: value subtree for the selected alternative.
    f.children.push_back(buildField(
      "value", std::string(variantFields[selectedIdx].description), variantFields[selectedIdx].type, innerPrefill));

    f.selectedVariantIndex = selectedIdx;
    return f;
  }

  // Scalar leaf.
  f.editor = classifyEditor(type);

  if (prefill != nullptr)
  {
    f.text = formatInlineArg(*prefill, type);

    // Strip outer quotes from formatInlineArg output; the form editor works with bare text.
    const bool needsUnquote =
      (f.editor == EditorKind::enumeration || f.editor == EditorKind::boolean || f.editor == EditorKind::text) &&
      f.text.size() >= 2 && f.text.front() == '"' && f.text.back() == '"';
    if (needsUnquote)
    {
      f.text = f.text.substr(1, f.text.size() - 2);
    }
    f.userEdited = true;
  }
  else
  {
    f.text = defaultTextFor(type);
    f.userEdited = false;
  }
  return f;
}

void toggleBoolField(ArgFormField& leaf)
{
  if (leaf.editor != EditorKind::boolean)
  {
    return;
  }
  leaf.text = (leaf.text == "true") ? "false" : "true";
  leaf.userEdited = true;
  leaf.validationError.clear();
}

namespace
{

/// Cycle through registered units in the quantity's category.
void cycleUnitField(ArgFormField& leaf, bool forward)
{
  if (leaf.editor != EditorKind::unitSelector || !leaf.type.has_value())
  {
    return;
  }
  const auto* quantityType = leaf.type.value()->asQuantityType();
  if (quantityType == nullptr)
  {
    return;
  }
  const auto canonicalUnit = quantityType->getUnit();
  if (!canonicalUnit.has_value())
  {
    return;
  }
  const auto units = UnitRegistry::get().getUnitsByCategory((*canonicalUnit)->getCategory());
  if (units.empty())
  {
    return;
  }

  // Find current unit; default to 0 if text doesn't match any known abbreviation.
  std::size_t currentIdx = 0;
  for (std::size_t i = 0; i < units.size(); ++i)
  {
    if (units[i]->getAbbreviation() == leaf.text)
    {
      currentIdx = i;
      break;
    }
  }
  const std::size_t nextIdx =
    forward ? (currentIdx + 1) % units.size() : (currentIdx + units.size() - 1) % units.size();
  leaf.text = std::string(units[nextIdx]->getAbbreviation());
  leaf.userEdited = true;
  leaf.validationError.clear();
}

}  // namespace

void cycleEnumField(ArgFormField& leaf, bool forward)
{
  if (leaf.editor != EditorKind::enumeration || !leaf.type.has_value())
  {
    return;
  }
  // Unwrap alias/optional to the EnumType.
  ConstTypeHandle<> t = *leaf.type;
  for (auto* a = t->asAliasType(); a != nullptr; a = t->asAliasType())
  {
    t = a->getAliasedType();
  }
  for (auto* o = t->asOptionalType(); o != nullptr; o = t->asOptionalType())
  {
    t = o->getType();
  }
  const auto* enumType = t->asEnumType();
  if (enumType == nullptr)
  {
    return;
  }
  const auto enums = enumType->getEnums();
  if (enums.empty())
  {
    return;
  }

  std::size_t currentIdx = 0;
  for (std::size_t i = 0; i < enums.size(); ++i)
  {
    if (enums[i].name == leaf.text)
    {
      currentIdx = i;
      break;
    }
  }

  std::size_t nextIdx = forward ? (currentIdx + 1) % enums.size() : (currentIdx + enums.size() - 1) % enums.size();
  leaf.text = std::string(enums[nextIdx].name);
  leaf.userEdited = true;
  leaf.validationError.clear();
}

namespace
{

/// Pre-order walk: collect leaf pointers.
void collectLeaves(ArgFormField& field, std::vector<ArgFormField*>& out)
{
  if (field.isLeaf())
  {
    out.push_back(&field);
    return;
  }
  for (auto& c: field.children)
  {
    collectLeaves(c, out);
  }
}

}  // namespace

void ArgForm::rebuildLeafCache()
{
  leaves_.clear();
  for (auto& f: fields_)
  {
    collectLeaves(f, leaves_);
  }
}

std::optional<ArgForm> ArgForm::build(const Method& method, std::string_view objectName, Span<const Var> prefilled)
{
  if (!methodIsFormCompatible(method))
  {
    return std::nullopt;
  }

  ArgForm form;
  form.method_ = &method;
  form.objectName_ = objectName;
  form.methodName_ = method.getName();

  const auto methodArgs = method.getArgs();
  // Reserve for pointer stability (leaf cache holds raw pointers).
  form.fields_.reserve(methodArgs.size());

  for (std::size_t i = 0; i < methodArgs.size(); ++i)
  {
    const auto& arg = methodArgs[i];
    const Var* prefill = (i < prefilled.size()) ? &prefilled[i] : nullptr;
    form.fields_.push_back(buildField(arg.name, arg.description, arg.type, prefill));
  }

  form.rebuildLeafCache();

  // Focus the first leaf of the first non-prefilled arg.
  std::size_t focused = 0;
  std::size_t leafCursor = 0;
  for (std::size_t i = 0; i < form.fields_.size(); ++i)
  {
    std::vector<ArgFormField*> subtreeLeaves;
    collectLeaves(form.fields_[i], subtreeLeaves);
    if (i >= prefilled.size())
    {
      focused = leafCursor;
      break;
    }
    leafCursor += subtreeLeaves.size();
  }
  if (!form.leaves_.empty() && focused >= form.leaves_.size())
  {
    focused = form.leaves_.size() - 1;
  }
  form.focused_ = focused;

  // Prime validation state.
  for (auto* leaf: form.leaves_)
  {
    std::ignore = form.revalidateLeaf(*leaf);
  }

  return form;
}

void ArgForm::focusNext() noexcept
{
  if (leaves_.empty())
  {
    return;
  }
  focused_ = (focused_ + 1) % leaves_.size();
}

void ArgForm::focusPrev() noexcept
{
  if (leaves_.empty())
  {
    return;
  }
  focused_ = (focused_ + leaves_.size() - 1) % leaves_.size();
}

void ArgForm::insertText(std::string_view s)
{
  if (leaves_.empty())
  {
    return;
  }
  auto& leaf = *leaves_[focused_];
  // Clear placeholder on first keystroke so "0" + typing "5" yields "5", not "05".
  if (!leaf.userEdited)
  {
    leaf.text.clear();
  }
  // Per-keystroke type filter.
  const auto filtered = filterAcceptableChars(s, leaf.type, leaf.text);
  if (filtered.empty() && !s.empty())
  {
    // All chars filtered; mark touched but don't change buffer.
    leaf.userEdited = true;
    return;
  }
  leaf.text += filtered;
  leaf.userEdited = true;
  std::ignore = revalidateLeaf(leaf);
}

void ArgForm::backspace()
{
  if (leaves_.empty())
  {
    return;
  }
  auto& leaf = *leaves_[focused_];
  // Clear placeholder on first backspace, same as insertText.
  if (!leaf.userEdited)
  {
    leaf.text.clear();
  }
  else
  {
    truncateOneCodePoint(leaf.text);
  }
  leaf.userEdited = true;
  std::ignore = revalidateLeaf(leaf);
}

void ArgForm::clearField()
{
  if (leaves_.empty())
  {
    return;
  }
  auto& leaf = *leaves_[focused_];
  leaf.text.clear();
  leaf.userEdited = true;
  std::ignore = revalidateLeaf(leaf);
}

void ArgForm::toggleFocused()
{
  if (leaves_.empty())
  {
    return;
  }
  auto& leaf = *leaves_[focused_];
  if (leaf.editor == EditorKind::boolean)
  {
    toggleBoolField(leaf);
    std::ignore = revalidateLeaf(leaf);
  }
}

void ArgForm::cycleFocused(bool forward)
{
  if (leaves_.empty())
  {
    return;
  }
  auto& leaf = *leaves_[focused_];
  if (leaf.editor == EditorKind::boolean)
  {
    toggleBoolField(leaf);
    std::ignore = revalidateLeaf(leaf);
  }
  else if (leaf.editor == EditorKind::enumeration)
  {
    cycleEnumField(leaf, forward);
    std::ignore = revalidateLeaf(leaf);
  }
  else if (leaf.editor == EditorKind::variantType)
  {
    cycleVariantType(forward);
  }
  else if (leaf.editor == EditorKind::integerSpin)
  {
    // Integer: +/-1 spin, clamped to type range.
    spinIntegerLeaf(leaf, forward ? 1 : -1);
    std::ignore = revalidateLeaf(leaf);
  }
  else if (leaf.editor == EditorKind::unitSelector)
  {
    // Cycle unit (interpretation, not value conversion).
    cycleUnitField(leaf, forward);
  }
}

EditorKind ArgForm::focusedEditor() const noexcept
{
  if (leaves_.empty())
  {
    return EditorKind::text;
  }
  return leaves_[focused_]->editor;
}

void ArgForm::cycleVariantType(bool forward)
{
  if (leaves_.empty())
  {
    return;
  }
  ArgFormField& typeLeaf = *leaves_[focused_];
  if (typeLeaf.editor != EditorKind::variantType || !typeLeaf.type.has_value())
  {
    return;
  }
  const auto* variantType = typeLeaf.type.value()->asVariantType();
  if (variantType == nullptr)
  {
    return;
  }

  // Find the variant group owning this selector leaf (always children[0]).
  ArgFormField* parent = nullptr;
  std::function<bool(ArgFormField&)> walk = [&](ArgFormField& node) -> bool
  {
    if (!node.children.empty() && &node.children[0] == &typeLeaf)
    {
      parent = &node;
      return true;
    }
    for (auto& c: node.children)
    {
      if (walk(c))
      {
        return true;
      }
    }
    return false;
  };
  for (auto& f: fields_)
  {
    if (walk(f))
    {
      break;
    }
  }
  if (parent == nullptr || parent->kind != FieldKind::variantGroup || parent->children.size() < 2)
  {
    return;
  }

  const auto variantFields = variantType->getFields();
  if (variantFields.empty())
  {
    return;
  }

  const std::size_t previousIdx = parent->selectedVariantIndex;
  const std::size_t nextIdx = forward ? (previousIdx + 1) % variantFields.size()
                                      : (previousIdx + variantFields.size() - 1) % variantFields.size();
  parent->selectedVariantIndex = nextIdx;
  typeLeaf.text = std::string(variantFields[nextIdx].type->getName());
  typeLeaf.userEdited = true;
  typeLeaf.validationError.clear();

  // Replace value subtree; reserve(2) ensures no reallocation.
  parent->children[1] =
    buildField("value", std::string(variantFields[nextIdx].description), variantFields[nextIdx].type, nullptr);

  rebuildLeafCache();

  // Keep focus on the type selector.
  for (std::size_t i = 0; i < leaves_.size(); ++i)
  {
    if (leaves_[i] == &typeLeaf)
    {
      focused_ = i;
      return;
    }
  }
}

namespace
{

/// Unwrap alias to the concrete type.
ConstTypeHandle<> unwrapAlias(MaybeConstTypeHandle<> t)
{
  while (t.has_value())
  {
    auto* a = t.value()->asAliasType();
    if (a == nullptr)
    {
      break;
    }
    t = a->getAliasedType();
  }
  return t.value();
}

bool isSequenceTypeField(const ArgFormField& f) { return unwrapAlias(f.type)->isSequenceType(); }

/// DFS walk; invokes `onFound(stack)` when `target` is reached.
bool walkToLeaf(ArgFormField& node,
                const ArgFormField* target,
                std::vector<ArgFormField*>& stack,
                const std::function<void(std::vector<ArgFormField*>&)>& onFound)
{
  stack.push_back(&node);
  if (&node == target)
  {
    onFound(stack);
    return true;
  }
  for (auto& child: node.children)
  {
    if (walkToLeaf(child, target, stack, onFound))
    {
      return true;
    }
  }
  stack.pop_back();
  return false;
}

/// Find the deepest sequence ancestor of `target`, or nullptr.
ArgFormField* findNearestSequenceAncestor(std::vector<ArgFormField>& topLevel, const ArgFormField* target)
{
  ArgFormField* result = nullptr;
  std::vector<ArgFormField*> stack;
  for (auto& f: topLevel)
  {
    if (walkToLeaf(f,
                   target,
                   stack,
                   [&](std::vector<ArgFormField*>& found)
                   {
                     for (auto it = found.rbegin() + 1; it != found.rend(); ++it)
                     {
                       if (isSequenceTypeField(**it))
                       {
                         result = *it;
                         return;
                       }
                     }
                   }))
    {
      break;
    }
    stack.clear();
  }
  return result;
}

/// Index of the first leaf in `subtreeRoot` within the leaf cache, or `noIndex` if not found.
std::size_t firstLeafOf(const std::vector<ArgFormField*>& leaves, const ArgFormField* subtreeRoot)
{
  const ArgFormField* firstLeaf = nullptr;
  std::function<void(const ArgFormField&)> walk = [&](const ArgFormField& f)
  {
    if (firstLeaf != nullptr)
    {
      return;
    }
    if (f.isLeaf())
    {
      firstLeaf = &f;
      return;
    }
    for (const auto& c: f.children)
    {
      walk(c);
      if (firstLeaf != nullptr)
      {
        return;
      }
    }
  };
  walk(*subtreeRoot);

  if (firstLeaf == nullptr)
  {
    return noIndex;
  }
  for (std::size_t i = 0; i < leaves.size(); ++i)
  {
    if (leaves[i] == firstLeaf)
    {
      return i;
    }
  }
  return noIndex;
}

}  // namespace

void ArgForm::addElementToFocusedSequence()
{
  // Target: nearest sequence ancestor, or first empty top-level sequence.
  ArgFormField* seq = nullptr;
  if (!leaves_.empty())
  {
    seq = findNearestSequenceAncestor(fields_, leaves_[focused_]);
  }
  if (seq == nullptr)
  {
    for (auto& f: fields_)
    {
      if (isSequenceTypeField(f) && f.children.empty())
      {
        seq = &f;
        break;
      }
    }
  }
  if (seq == nullptr)
  {
    return;
  }

  const auto* seqType = unwrapAlias(seq->type)->asSequenceType();
  if (seqType == nullptr)
  {
    return;
  }
  if (auto maxSize = seqType->getMaxSize(); maxSize.has_value() && seq->children.size() >= *maxSize)
  {
    return;
  }

  const std::size_t newIndex = seq->children.size();
  seq->children.push_back(buildField("[" + std::to_string(newIndex) + "]", "", seqType->getElementType(), nullptr));

  rebuildLeafCache();

  // Focus the new element's first leaf.
  const auto newLeafIdx = firstLeafOf(leaves_, &seq->children.back());
  if (newLeafIdx != noIndex)
  {
    focused_ = newLeafIdx;
  }
  else if (!leaves_.empty() && focused_ >= leaves_.size())
  {
    focused_ = leaves_.size() - 1;
  }
}

namespace
{

/// Find the nearest optionalGroup ancestor of `target`, or nullptr.
ArgFormField* findNearestOptionalAncestor(std::vector<ArgFormField>& topLevel, const ArgFormField* target)
{
  ArgFormField* result = nullptr;
  std::vector<ArgFormField*> stack;
  for (auto& f: topLevel)
  {
    const bool found = walkToLeaf(f,
                                  target,
                                  stack,
                                  [&](std::vector<ArgFormField*>& s)
                                  {
                                    for (auto it = s.rbegin() + 1; it != s.rend(); ++it)
                                    {
                                      if ((*it)->kind == FieldKind::optionalGroup)
                                      {
                                        result = *it;
                                        return;
                                      }
                                    }
                                  });
    if (found)
    {
      break;
    }
    stack.clear();
  }
  return result;
}

}  // namespace

void ArgForm::toggleFocusedOptional()
{
  // Target: nearest optional ancestor, or first empty top-level optional.
  ArgFormField* opt = nullptr;
  if (!leaves_.empty())
  {
    opt = findNearestOptionalAncestor(fields_, leaves_[focused_]);
  }
  if (opt == nullptr)
  {
    for (auto& f: fields_)
    {
      if (f.kind == FieldKind::optionalGroup && f.optionalIsEmpty)
      {
        opt = &f;
        break;
      }
    }
  }
  if (opt == nullptr || !opt->type.has_value())
  {
    return;
  }

  const auto* optType = opt->type.value()->asOptionalType();
  if (optType == nullptr)
  {
    return;
  }

  if (opt->optionalIsEmpty)
  {
    // Empty -> filled.
    opt->children.clear();
    opt->children.push_back(buildField("value", "", optType->getType(), nullptr));
    opt->optionalIsEmpty = false;
  }
  else
  {
    // Filled -> empty.
    opt->children.clear();
    opt->optionalIsEmpty = true;
  }

  rebuildLeafCache();

  // Focus first leaf of the new inner, or clamp to nearest remaining.
  if (!opt->optionalIsEmpty && !opt->children.empty())
  {
    const auto idx = firstLeafOf(leaves_, &opt->children.front());
    if (idx != noIndex)
    {
      focused_ = idx;
      return;
    }
  }
  if (leaves_.empty())
  {
    focused_ = 0;
    return;
  }
  focused_ = std::min(focused_, leaves_.size() - 1);
}

void ArgForm::removeFocusedSequenceElement()
{
  if (leaves_.empty())
  {
    return;
  }

  // Find the sequence ancestor containing the focused leaf.
  ArgFormField* seq = findNearestSequenceAncestor(fields_, leaves_[focused_]);
  if (seq == nullptr)
  {
    return;
  }

  // Fixed-size arrays: refuse removal.
  if (const auto* seqType = unwrapAlias(seq->type)->asSequenceType(); seqType != nullptr && seqType->hasFixedSize())
  {
    return;
  }

  // Find which direct child of seq contains the focused leaf.
  const ArgFormField* focused = leaves_[focused_];
  std::size_t elementIndex = seq->children.size();  // sentinel: "not found"
  for (std::size_t i = 0; i < seq->children.size(); ++i)
  {
    bool found = false;
    std::vector<ArgFormField*> stack;
    walkToLeaf(seq->children[i], focused, stack, [&](std::vector<ArgFormField*>&) { found = true; });
    if (found)
    {
      elementIndex = i;
      break;
    }
  }
  if (elementIndex == seq->children.size())
  {
    return;
  }

  seq->children.erase(seq->children.begin() + checkedConversion<ptrdiff_t>(elementIndex));

  // Re-index element names.
  for (std::size_t i = 0; i < seq->children.size(); ++i)
  {
    seq->children[i].name = "[" + std::to_string(i) + "]";
  }

  rebuildLeafCache();

  // Focus: element at removed index -> previous element -> any leaf -> 0.
  std::size_t newFocus = noIndex;
  if (elementIndex < seq->children.size())
  {
    newFocus = firstLeafOf(leaves_, &seq->children[elementIndex]);
  }
  if (newFocus == noIndex && elementIndex > 0)
  {
    newFocus = firstLeafOf(leaves_, &seq->children[elementIndex - 1]);
  }
  if (newFocus == noIndex)
  {
    newFocus = leaves_.empty() ? 0 : std::min(focused_, leaves_.size() - 1);
  }
  focused_ = leaves_.empty() ? 0 : newFocus;
}

bool ArgForm::focusedIsInsideSequence() const noexcept
{
  // Mirrors addElementToFocusedSequence dispatch: ancestor or empty top-level sequence.
  auto& mutableFields = const_cast<std::vector<ArgFormField>&>(fields_);
  if (!leaves_.empty() && findNearestSequenceAncestor(mutableFields, leaves_[focused_]) != nullptr)
  {
    return true;
  }
  for (const auto& f: fields_)
  {
    if (isSequenceTypeField(f) && f.children.empty())
    {
      return true;
    }
  }
  return false;
}

bool ArgForm::focusedCanAddElement() const noexcept
{
  // Same dispatch as addElementToFocusedSequence.
  auto& mutableFields = const_cast<std::vector<ArgFormField>&>(fields_);
  ArgFormField* seq = nullptr;
  if (!leaves_.empty())
  {
    seq = findNearestSequenceAncestor(mutableFields, leaves_[focused_]);
  }
  if (seq == nullptr)
  {
    for (auto& f: mutableFields)
    {
      if (isSequenceTypeField(f) && f.children.empty())
      {
        seq = &f;
        break;
      }
    }
  }
  if (seq == nullptr || !seq->type.has_value())
  {
    return false;
  }
  const auto* seqType = unwrapAlias(seq->type)->asSequenceType();
  if (seqType == nullptr)
  {
    return false;
  }
  if (seqType->hasFixedSize())
  {
    return false;
  }
  if (auto max = seqType->getMaxSize(); max.has_value() && seq->children.size() >= *max)
  {
    return false;
  }
  return true;
}

bool ArgForm::focusedCanRemoveElement() const noexcept
{
  if (leaves_.empty())
  {
    return false;
  }
  auto& mutableFields = const_cast<std::vector<ArgFormField>&>(fields_);
  ArgFormField* seq = findNearestSequenceAncestor(mutableFields, leaves_[focused_]);
  if (seq == nullptr || !seq->type.has_value())
  {
    return false;
  }
  const auto* seqType = unwrapAlias(seq->type)->asSequenceType();
  if (seqType == nullptr)
  {
    return false;
  }
  return !seqType->hasFixedSize();
}

bool ArgForm::focusedIsInsideOptional() const noexcept
{
  // Mirrors focusedIsInsideSequence logic but for optionalGroup.
  auto& mutableFields = const_cast<std::vector<ArgFormField>&>(fields_);
  if (!leaves_.empty() && findNearestOptionalAncestor(mutableFields, leaves_[focused_]) != nullptr)
  {
    return true;
  }
  for (const auto& f: fields_)
  {
    if (f.kind == FieldKind::optionalGroup && f.optionalIsEmpty)
    {
      return true;
    }
  }
  return false;
}

std::optional<Var> ArgForm::assembleValue(ArgFormField& f, std::size_t& failIdx)
{
  if (f.isLeaf())
  {
    auto parsed = revalidateLeaf(f);
    if (!parsed.has_value())
    {
      for (std::size_t i = 0; i < leaves_.size(); ++i)
      {
        if (leaves_[i] == &f)
        {
          failIdx = i;
          break;
        }
      }
      return std::nullopt;
    }
    return parsed;
  }

  // Composite dispatch.
  auto t = unwrapAlias(f.type);

  if (t->isSequenceType())
  {
    VarList list;
    list.reserve(f.children.size());
    for (auto& child: f.children)
    {
      auto v = assembleValue(child, failIdx);
      if (!v.has_value())
      {
        return std::nullopt;
      }
      list.push_back(std::move(*v));
    }
    return Var(std::move(list));
  }

  if (f.kind == FieldKind::optionalGroup)
  {
    // Empty optional -> monostate.
    if (f.optionalIsEmpty || f.children.empty())
    {
      return Var {};
    }
    return assembleValue(f.children[0], failIdx);
  }

  if (f.kind == FieldKind::quantityGroup)
  {
    // Convert user's value from selected unit to canonical unit, emit as f64.
    if (f.children.size() < 2)
    {
      return std::nullopt;
    }
    auto& valueLeaf = f.children[0];
    auto& unitLeaf = f.children[1];

    auto parsedValue = revalidateLeaf(valueLeaf);
    if (!parsedValue.has_value())
    {
      for (std::size_t i = 0; i < leaves_.size(); ++i)
      {
        if (leaves_[i] == &valueLeaf)
        {
          failIdx = i;
          break;
        }
      }
      return std::nullopt;
    }

    const auto* quantityType = (f.type.has_value()) ? f.type.value()->asQuantityType() : nullptr;
    if (quantityType == nullptr)
    {
      return std::nullopt;
    }
    const auto canonicalUnit = quantityType->getUnit();
    if (!canonicalUnit.has_value())
    {
      return parsedValue;  // Unit-less quantities don't build a quantityGroup.
    }

    const auto selected = UnitRegistry::get().searchUnitByAbbreviation(unitLeaf.text);
    if (!selected.has_value())
    {
      unitLeaf.validationError = "unknown unit: " + unitLeaf.text;
      for (std::size_t i = 0; i < leaves_.size(); ++i)
      {
        if (leaves_[i] == &unitLeaf)
        {
          failIdx = i;
          break;
        }
      }
      return std::nullopt;
    }
    unitLeaf.validationError.clear();

    const auto userValue = parsedValue->getCopyAs<float64_t>();
    const auto canonicalValue = Unit::convert(**selected, **canonicalUnit, userValue);
    return Var(canonicalValue);
  }

  if (const auto* variantType = t->asVariantType(); variantType != nullptr)
  {
    // Build {"type": "...", "value": ...}; adaptVariant turns this into a KeyedVar.
    if (f.children.size() < 2)
    {
      return std::nullopt;
    }
    auto& valueChild = f.children[1];
    auto innerVar = assembleValue(valueChild, failIdx);
    if (!innerVar.has_value())
    {
      return std::nullopt;
    }
    const auto fields = variantType->getFields();
    if (f.selectedVariantIndex >= fields.size())
    {
      return std::nullopt;
    }
    VarMap map;
    map.try_emplace("type", std::string(fields[f.selectedVariantIndex].type->getName()));
    map.try_emplace("value", std::move(*innerVar));
    return Var(std::move(map));
  }

  // Struct: VarMap keyed by child name.
  VarMap map;
  for (auto& child: f.children)
  {
    auto v = assembleValue(child, failIdx);
    if (!v.has_value())
    {
      return std::nullopt;
    }
    map.try_emplace(child.name, std::move(*v));
  }
  return Var(std::move(map));
}

Result<VarList, ArgForm::SubmitError> ArgForm::trySubmit()
{
  VarList values;
  values.reserve(fields_.size());
  std::size_t failIdx = 0;
  for (auto& f: fields_)
  {
    auto v = assembleValue(f, failIdx);
    if (!v.has_value())
    {
      return Err(SubmitError {failIdx, leaves_[failIdx]->validationError});
    }
    values.push_back(std::move(*v));
  }
  return Ok(std::move(values));
}

std::optional<Var> ArgForm::revalidateLeaf(ArgFormField& leaf)
{
  // Unit and variant-type selectors are not value leaves.
  if (leaf.editor == EditorKind::unitSelector || leaf.editor == EditorKind::variantType)
  {
    leaf.validationError.clear();
    return Var {};
  }
  auto r = parseFieldText(leaf.text, *leaf.type.value());
  if (r.isError())
  {
    leaf.validationError = r.getError();
    return std::nullopt;
  }
  leaf.validationError.clear();
  return r.getValue();
}

}  // namespace sen::components::term
