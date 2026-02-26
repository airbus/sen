// === var.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_VAR_H
#define SEN_CORE_META_VAR_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type_traits.h"

// std
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sen
{

/// \addtogroup type_utils
/// @{

/// We need to forward-declare this.
struct Var;

//--------------------------------------------------------------------------------------------------------------
// Conversion functions
//--------------------------------------------------------------------------------------------------------------

/// Converts a variant into its Json representation.
/// See https://www.Json.org/Json-en.html for details
[[nodiscard]] std::string toJson(const Var& var, int indent = 2);

/// Inverse as toJson
[[nodiscard]] Var fromJson(const std::string& str);

/// Converts a variant into its bson (Binary JSON) representation.
/// See https://bsonspec.org/ for details.
[[nodiscard]] std::vector<std::uint8_t> toBson(const Var& var);

/// Inverse as toBson
[[nodiscard]] Var fromBson(const std::vector<std::uint8_t>& bson);

/// Converts a variant into its MessagePack representation.
/// See https://msgpack.org/ for details.
[[nodiscard]] std::vector<std::uint8_t> toCbor(const Var& var);

/// Inverse as toCbor
[[nodiscard]] Var fromCbor(const std::vector<std::uint8_t>& cbor);

/// Converts a variant into its msgpack representation.
/// See https://cbor.io/ for details.
[[nodiscard]] std::vector<std::uint8_t> toMsgpack(const Var& var);

/// Inverse as toMsgpack
[[nodiscard]] Var fromMsgpack(const std::vector<std::uint8_t>& msgpack);

/// Converts a variant into its UBJSON (Universal Binary JSON Specification) representation.
/// See https://ubjson.org/ for details.
[[nodiscard]] std::vector<std::uint8_t> toUbjson(const Var& var);

/// Inverse as toUbjson
[[nodiscard]] Var fromUbjson(const std::vector<std::uint8_t>& ubson);

/// Converts a string to a duration
[[nodiscard]] Duration stringToDuration(const std::string& str);

/// Tries to transform the stored value to T.
/// For expensive types, like strings, maps or lists is better use:
///   - a visitor, or
///   - is<T>() together with tryGet<T>().
///
/// Rules:
///   - Empty Vars generate T{}
///   - Native type conversion use c++ rules.
///   - If T is std::string and the var holds a bool, it returns "true" or "false"
///   - If T is bool and the var holds a std::string, it returns true if the string is "true"
///   - Durations can be converted to: strings (nanoseconds), timestamps and integrals
///   - Timestamps can be converted to: strings (nanoseconds), durations and integrals
///   - Lists and maps can only be converted to strings
///
/// Throws std::exception if the conversion could not be made
///
/// @tparam To: type to transform to
/// @tparam checkedConversion: whether conversions should be checked against precision loss/truncation
///
/// @param var: Var containing the value that should be converted
template <typename To, bool checkedConversion = false>
[[nodiscard]] To getCopyAs(const Var& var);

/// A list of vars to represent sequences.
using VarList = std::vector<Var>;

/// A map of vars to represent structures.
using VarMap = std::map<std::string, Var, std::less<>>;

/// A key-var tuple, to represent variants.
using KeyedVar = std::tuple<uint32_t, std::shared_ptr<Var>>;

//--------------------------------------------------------------------------------------------------------------
// Var
//--------------------------------------------------------------------------------------------------------------

/// Can hold any supported value type.
/// Wraps std::variant to allow recursion and implements some helpers.
struct Var
{
  SEN_COPY_MOVE(Var)

public:
  /// The std::variant that we wrap.
  using ValueType = std::variant<std::monostate,
                                 int32_t,
                                 uint32_t,
                                 int64_t,
                                 uint64_t,
                                 float32_t,
                                 float64_t,
                                 Duration,
                                 TimeStamp,
                                 uint8_t,
                                 int16_t,
                                 uint16_t,
                                 bool,
                                 std::string,
                                 VarList,
                                 VarMap,
                                 KeyedVar>;

public:  // default construction and destruction
  Var() = default;
  ~Var() = default;

public:
  /// Construction from a value (explicit).
  template <typename T>
  constexpr Var(T t)  // NOLINT
  {
    if constexpr (std::is_move_constructible_v<T>)
    {
      value_ = ValueType(std::move(t));
    }
    else
    {
      value_ = ValueType(t);
    }
  }

public:
  /// Conversion into the wrapped std::variant for interoperability.
  constexpr operator ValueType&() noexcept  // NOLINT
  {
    return value_;
  }

  /// Conversion into the wrapped std::variant for interoperability.
  constexpr operator const ValueType&() const noexcept  // NOLINT
  {
    return value_;
  }

public:
  /// True if the Var holds a value of T.
  template <typename T>
  [[nodiscard]] constexpr bool holds() const noexcept
  {
    return std::holds_alternative<T>(value_);
  }

  /// True if the Var holds a value of type T, where T is in Ts .
  template <typename... Ts>
  [[nodiscard]] constexpr bool holdsAnyOff() const noexcept
  {
    return (holds<Ts>() || ...);
  }

  /// Checks whether the contained value is a integral type.
  [[nodiscard]] constexpr bool holdsIntegralValue() const noexcept
  {
    return holdsAnyOff<int32_t, uint32_t, int64_t, uint64_t, uint8_t, int16_t, uint16_t, bool>();
  }

  /// Checks whether the contained value is a floating point type.
  [[nodiscard]] constexpr bool holdsFloatingPointValue() const noexcept { return holdsAnyOff<float32_t, float64_t>(); }

  /// True if the Var holds a value of std::monostate.
  [[nodiscard]] constexpr bool isEmpty() const noexcept { return std::holds_alternative<std::monostate>(value_); }

  /// See sen::getCopyAs().
  template <typename T, bool checkedConversion = false>
  [[nodiscard]] T getCopyAs() const
  {
    return ::sen::getCopyAs<T>(*this);
  }

  /// Same as std::get<T>(this->value);.
  template <typename T>
  [[nodiscard]] constexpr T& get()
  {
    return std::get<T>(value_);
  }

  /// Same as std::get<T>(this->value);.
  template <typename T>
  [[nodiscard]] constexpr const T& get() const
  {
    return std::get<T>(value_);
  }

  /// Same as std::get_if<T>(&(this->value)).
  template <typename T>
  [[nodiscard]] const T* getIf() const noexcept
  {
    return std::get_if<T>(&value_);
  }

  /// Same as std::get_if<T>(&(this->value)).
  template <typename T>
  [[nodiscard]] T* getIf() noexcept
  {
    return std::get_if<T>(&value_);
  }

  /// Swaps contents with another Var.
  void swap(Var& rhs) noexcept;

  [[nodiscard]] friend bool operator==(const Var& lhs, const Var& rhs) noexcept { return lhs.isEqual(rhs); }
  [[nodiscard]] friend bool operator!=(const Var& lhs, const Var& rhs) noexcept { return !lhs.isEqual(rhs); }

private:
  [[nodiscard]] bool isEqual(const Var& rhs) const noexcept;

private:
  ValueType value_ {};
};

template <typename T>
struct IsVariantMember<T, Var>: public IsVariantMember<T, typename Var::ValueType>
{
};

template <typename T>
constexpr bool isVarTypeMemberV = IsVariantMember<T, Var>::value;

/// Finds an element in a map or throws std::exception
/// containing the provided string otherwise.
[[nodiscard]] const Var& findElement(const VarMap& map, const std::string& key, const std::string& errorMessage);

/// Finds an element in a map or throws std::exception
/// containing the provided string otherwise.
template <typename T>
[[nodiscard]] T findElementAs(const VarMap& map, const std::string& key, const std::string& errorMessage)
{
  return getCopyAs<T>(findElement(map, key, errorMessage));
}

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace impl
{

void getCopyAsImpl(const Var& var, std::monostate& val);
void getCopyAsImpl(const Var& var, int32_t& val);
void getCopyAsImpl(const Var& var, uint32_t& val);
void getCopyAsImpl(const Var& var, int64_t& val);
void getCopyAsImpl(const Var& var, uint64_t& val);
void getCopyAsImpl(const Var& var, float32_t& val);
void getCopyAsImpl(const Var& var, float64_t& val);
void getCopyAsImpl(const Var& var, Duration& val);
void getCopyAsImpl(const Var& var, TimeStamp& val);
void getCopyAsImpl(const Var& var, uint8_t& val);
void getCopyAsImpl(const Var& var, int16_t& val);
void getCopyAsImpl(const Var& var, uint16_t& val);
void getCopyAsImpl(const Var& var, bool& val);
void getCopyAsImpl(const Var& var, std::string& val);
void getCopyAsImpl(const Var& var, VarList& val);
void getCopyAsImpl(const Var& var, VarMap& val);
void getCopyAsImpl(const Var& var, KeyedVar& val);

void getCheckedCopyAsImpl(const Var& var, std::monostate& val);
void getCheckedCopyAsImpl(const Var& var, int32_t& val);
void getCheckedCopyAsImpl(const Var& var, uint32_t& val);
void getCheckedCopyAsImpl(const Var& var, int64_t& val);
void getCheckedCopyAsImpl(const Var& var, uint64_t& val);
void getCheckedCopyAsImpl(const Var& var, float32_t& val);
void getCheckedCopyAsImpl(const Var& var, float64_t& val);
void getCheckedCopyAsImpl(const Var& var, Duration& val);
void getCheckedCopyAsImpl(const Var& var, TimeStamp& val);
void getCheckedCopyAsImpl(const Var& var, uint8_t& val);
void getCheckedCopyAsImpl(const Var& var, int16_t& val);
void getCheckedCopyAsImpl(const Var& var, uint16_t& val);
void getCheckedCopyAsImpl(const Var& var, bool& val);
void getCheckedCopyAsImpl(const Var& var, std::string& val);
void getCheckedCopyAsImpl(const Var& var, VarList& val);
void getCheckedCopyAsImpl(const Var& var, VarMap& val);
void getCheckedCopyAsImpl(const Var& var, KeyedVar& val);

}  // namespace impl

//--------------------------------------------------------------------------------------------------------------
// Free functions
//--------------------------------------------------------------------------------------------------------------

template <typename To, bool checkedConversion>
inline To getCopyAs(const Var& var)
{
  if constexpr (isVarTypeMemberV<To>)
  {
    // don't transform if already the same type
    if (var.holds<To>())
    {
      return var.get<To>();
    }

    To value {};
    if constexpr (checkedConversion)
    {
      impl::getCheckedCopyAsImpl(var, value);
    }
    else
    {
      impl::getCopyAsImpl(var, value);
    }
    return value;
  }
  else
  {
    To value {};
    ::sen::VariantTraits<To>::variantToValue(var, value);
    return value;
  }
}

inline const Var& findElement(const VarMap& map, const std::string& key, const std::string& errorMessage)
{
  if (auto itr = map.find(key); itr != map.end())
  {
    return itr->second;
  }

  ::sen::throwRuntimeError(errorMessage);
  SEN_UNREACHABLE();
}

}  // namespace sen

#endif  // SEN_CORE_META_VAR_H
