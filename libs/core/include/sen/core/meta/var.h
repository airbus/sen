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

/// Serialises a `Var` to a JSON string.
/// @param var    The variant to serialise.
/// @param indent Number of spaces per indentation level (default 2; pass 0 for compact output).
/// @return JSON text representation.
[[nodiscard]] std::string toJson(const Var& var, int indent = 2);

/// Deserialises a JSON string into a `Var`.
/// @param str JSON text to parse.
/// @return The parsed `Var`.
/// @throws std::exception if `str` is not valid JSON.
[[nodiscard]] Var fromJson(const std::string& str);

/// Serialises a `Var` to BSON (Binary JSON) bytes.
/// @param var The variant to serialise.
/// @return BSON-encoded byte vector.
[[nodiscard]] std::vector<std::uint8_t> toBson(const Var& var);

/// Deserialises BSON bytes into a `Var`.
/// @param bson BSON-encoded bytes to parse.
/// @return The parsed `Var`.
[[nodiscard]] Var fromBson(const std::vector<std::uint8_t>& bson);

/// Serialises a `Var` to CBOR (Concise Binary Object Representation) bytes.
/// @param var The variant to serialise.
/// @return CBOR-encoded byte vector.
[[nodiscard]] std::vector<std::uint8_t> toCbor(const Var& var);

/// Deserialises CBOR bytes into a `Var`.
/// @param cbor CBOR-encoded bytes to parse.
/// @return The parsed `Var`.
[[nodiscard]] Var fromCbor(const std::vector<std::uint8_t>& cbor);

/// Serialises a `Var` to MessagePack bytes.
/// @param var The variant to serialise.
/// @return MessagePack-encoded byte vector.
[[nodiscard]] std::vector<std::uint8_t> toMsgpack(const Var& var);

/// Deserialises MessagePack bytes into a `Var`.
/// @param msgpack MessagePack-encoded bytes to parse.
/// @return The parsed `Var`.
[[nodiscard]] Var fromMsgpack(const std::vector<std::uint8_t>& msgpack);

/// Serialises a `Var` to UBJSON (Universal Binary JSON) bytes.
/// @param var The variant to serialise.
/// @return UBJSON-encoded byte vector.
[[nodiscard]] std::vector<std::uint8_t> toUbjson(const Var& var);

/// Deserialises UBJSON bytes into a `Var`.
/// @param ubson UBJSON-encoded bytes to parse.
/// @return The parsed `Var`.
[[nodiscard]] Var fromUbjson(const std::vector<std::uint8_t>& ubson);

/// Parses a human-readable duration string (e.g. `"100ms"`, `"1s"`) into a `Duration`.
/// @param str Duration string to parse.
/// @return The parsed `Duration`.
/// @throws std::exception if `str` is not a recognised duration format.
[[nodiscard]] Duration stringToDuration(const std::string& str);

/// Extracts or converts the value stored in `var` to type `To`.
///
/// For expensive types (strings, maps, lists) prefer using a visitor or `holds<T>()`
/// combined with `get<T>()` to avoid unnecessary copies.
///
/// Conversion rules:
///   - An empty `Var` (`std::monostate`) returns `To{}`.
///   - Between native numeric types, standard C++ conversion rules apply.
///   - `std::string` â†’ `bool`: `"true"` â†’ `true`, anything else â†’ `false`.
///   - `bool` â†’ `std::string`: `true` â†’ `"true"`, `false` â†’ `"false"`.
///   - `Duration` â†’ `std::string` / `TimeStamp` / integral: nanosecond count.
///   - `TimeStamp` â†’ `std::string` / `Duration` / integral: nanosecond count.
///   - `VarList` / `VarMap` can only be converted to `std::string` (JSON).
///
/// @tparam To                 Target type to convert to.
/// @tparam checkedConversion  If `true`, conversions that would lose precision or truncate throw.
/// @param var Variant holding the source value.
/// @return A copy of the value converted to `To`.
/// @throws std::exception if the conversion is not supported or fails a precision check.
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

/// Universal type-erased value container used throughout the Sen framework.
/// Wraps `std::variant` to allow recursive types (`VarList`, `VarMap`, `KeyedVar`)
/// and provides ergonomic helpers for type checking, extraction, and conversion.
///
/// Use `holds<T>()` to check the active type, `get<T>()` or `getIf<T>()` to access
/// the value, and `getCopyAs<T>()` for cross-type conversion.
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
  /// Returns `true` if this `Var` currently holds a value of type `T`.
  /// @tparam T One of the alternatives in `ValueType`.
  template <typename T>
  [[nodiscard]] constexpr bool holds() const noexcept
  {
    return std::holds_alternative<T>(value_);
  }

  /// Returns `true` if this `Var` holds any of the types in `Ts`.
  /// @tparam Ts Pack of alternatives to test against (e.g. `holdsAnyOff<int32_t, uint32_t>()`).
  template <typename... Ts>
  [[nodiscard]] constexpr bool holdsAnyOff() const noexcept
  {
    return (holds<Ts>() || ...);
  }

  /// Returns `true` if the active type is any integer type (including `bool`, `uint8_t`, etc.).
  [[nodiscard]] constexpr bool holdsIntegralValue() const noexcept
  {
    return holdsAnyOff<int32_t, uint32_t, int64_t, uint64_t, uint8_t, int16_t, uint16_t, bool>();
  }

  /// Returns `true` if the active type is `float32_t` or `float64_t`.
  [[nodiscard]] constexpr bool holdsFloatingPointValue() const noexcept { return holdsAnyOff<float32_t, float64_t>(); }

  /// Returns `true` if this `Var` is empty (holds `std::monostate`, the default-constructed state).
  [[nodiscard]] constexpr bool isEmpty() const noexcept { return std::holds_alternative<std::monostate>(value_); }

  /// Converts the stored value to `T`. See the free function `sen::getCopyAs()` for conversion rules.
  /// @tparam T                 Target type.
  /// @tparam checkedConversion If `true`, precision-losing conversions throw.
  /// @return Copy of the stored value converted to `T`.
  template <typename T, bool checkedConversion = false>
  [[nodiscard]] T getCopyAs() const
  {
    return ::sen::getCopyAs<T>(*this);
  }

  /// Returns a reference to the stored value of type `T`.
  /// @tparam T Alternative type to extract; must match the active alternative.
  /// @return Mutable reference to the stored value.
  /// @throws std::bad_variant_access if `T` is not the active alternative.
  template <typename T>
  [[nodiscard]] constexpr T& get()
  {
    return std::get<T>(value_);
  }

  /// Returns a const reference to the stored value of type `T`.
  /// @tparam T Alternative type to extract; must match the active alternative.
  /// @return Const reference to the stored value.
  /// @throws std::bad_variant_access if `T` is not the active alternative.
  template <typename T>
  [[nodiscard]] constexpr const T& get() const
  {
    return std::get<T>(value_);
  }

  /// Returns a pointer to the stored value of type `T`, or `nullptr` if `T` is not active.
  /// @tparam T Alternative type to test for.
  /// @return Const pointer to the stored value, or `nullptr`.
  template <typename T>
  [[nodiscard]] const T* getIf() const noexcept
  {
    return std::get_if<T>(&value_);
  }

  /// Returns a pointer to the stored value of type `T`, or `nullptr` if `T` is not active.
  /// @tparam T Alternative type to test for.
  /// @return Mutable pointer to the stored value, or `nullptr`.
  template <typename T>
  [[nodiscard]] T* getIf() noexcept
  {
    return std::get_if<T>(&value_);
  }

  /// Exchanges the contents of this `Var` with `rhs`.
  /// @param rhs The other `Var` to swap with.
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

/// Looks up a key in a `VarMap` and returns a reference to the associated value.
/// @param map          The map to search.
/// @param key          The key to look up.
/// @param errorMessage Message passed to the thrown exception if the key is absent.
/// @return Const reference to the found `Var`.
/// @throws std::runtime_error (with `errorMessage`) if `key` is not in `map`.
[[nodiscard]] const Var& findElement(const VarMap& map, const std::string& key, const std::string& errorMessage);

/// Looks up a key in a `VarMap` and returns the associated value converted to `T`.
/// @tparam T           Target type for the conversion (see `getCopyAs`).
/// @param map          The map to search.
/// @param key          The key to look up.
/// @param errorMessage Message passed to the thrown exception if the key is absent.
/// @return The found value converted to `T`.
/// @throws std::runtime_error (with `errorMessage`) if `key` is not in `map`.
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
