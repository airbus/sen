// === var.cpp =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/var.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type_traits.h"

// nlohmann
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"

// std
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

// json
#include "nlohmann/json.hpp"

using Json = nlohmann::json;

namespace sen
{
//--------------------------------------------------------------------------------------------------------------
// Var
//--------------------------------------------------------------------------------------------------------------

void Var::swap(Var& rhs) noexcept { value_.swap(rhs); }

bool Var::isEqual(const Var& rhs) const noexcept { return value_ == rhs.value_; }

namespace
{

//--------------------------------------------------------------------------------------------------------------
// jsonToVar
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] inline Var jsonToVar(const Json& val)
{
  switch (val.type())
  {
    case Json::value_t::null:  // null value
      return std::monostate {};

    case Json::value_t::object:  // unordered set of name/value pairs
    {
      VarMap result;
      for (const auto& el: val.items())
      {
        result.try_emplace(el.key(), jsonToVar(el.value()));
      }
      return result;
    }

    case Json::value_t::array:  // ordered collection of values
    {
      VarList result;
      result.reserve(val.size());

      for (const auto& elem: val)
      {
        result.push_back(jsonToVar(elem));
      }
      return result;
    }

    case Json::value_t::string:
      return val.get<std::string>();

    case Json::value_t::boolean:
      return val.get<bool>();

    case Json::value_t::number_integer:  // signed integer
      return val.get<int64_t>();

    case Json::value_t::number_unsigned:  // unsigned integer
      return val.get<uint64_t>();

    case Json::value_t::number_float:  // floating-point
      return val.get<float64_t>();

    default:
      throwRuntimeError("Json type not supported");
  }
}

[[nodiscard]] Json varToJson(const Var& var)
{
  return std::visit(Overloaded {[](std::monostate /*val*/) -> Json { return {}; },
                                [](auto val) -> Json { return val; },
                                [](const Duration& val) -> Json { return val.getNanoseconds(); },
                                [](const TimeStamp& val) -> Json { return val.toUtcString(); },
                                [](const VarList& val) -> Json
                                {
                                  Json result(nlohmann::json::value_t::array);
                                  for (const auto& elem: val)
                                  {
                                    result.emplace_back(varToJson(elem));
                                  }
                                  return result;
                                },
                                [](const VarMap& val) -> Json
                                {
                                  Json result(nlohmann::json::value_t::object);
                                  for (const auto& [key, elemVal]: val)
                                  {
                                    result[key] = varToJson(elemVal);
                                  }
                                  return result;
                                },
                                [](const KeyedVar& tuple) -> Json
                                {
                                  Json result(nlohmann::json::value_t::object);
                                  result["type"] = std::get<0U>(tuple);
                                  result["value"] = varToJson(*std::get<1U>(tuple));
                                  return result;
                                }},
                    static_cast<const Var::ValueType&>(var));
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Conversion functions
//--------------------------------------------------------------------------------------------------------------

std::string toJson(const Var& var, int indent) { return varToJson(var).dump(indent); }

Var fromJson(const std::string& str) { return jsonToVar(Json::parse(str)); }

std::vector<std::uint8_t> toBson(const Var& var) { return Json::to_bson(varToJson(var)); }

Var fromBson(const std::vector<std::uint8_t>& bson) { return jsonToVar(Json::from_bson(bson)); }

std::vector<std::uint8_t> toCbor(const Var& var) { return Json::to_cbor(varToJson(var)); }

Var fromCbor(const std::vector<std::uint8_t>& cbor) { return jsonToVar(Json::from_cbor(cbor)); }

std::vector<std::uint8_t> toMsgpack(const Var& var) { return Json::to_msgpack(varToJson(var)); }

Var fromMsgpack(const std::vector<std::uint8_t>& msgpack) { return jsonToVar(Json::from_msgpack(msgpack)); }

std::vector<std::uint8_t> toUbjson(const Var& var) { return Json::to_ubjson(varToJson(var)); }

Var fromUbjson(const std::vector<std::uint8_t>& ubson) { return jsonToVar(Json::from_ubjson(ubson)); }

Duration stringToDuration(const std::string& str)
{
  auto result = QuantityTraits<Duration>::unit().fromString(str);
  if (result.isOk())
  {
    return result.getValue();
  }

  throw std::runtime_error(result.getError());
}

namespace impl
{

//--------------------------------------------------------------------------------------------------------------
// Converter
//--------------------------------------------------------------------------------------------------------------

/// Helper visitor to convert types defined in Var
template <typename To, bool checkedConversion = false>
struct Converter
{
  /// Empty Vars generate a default-constructed value.
  [[nodiscard]] To operator()(std::monostate val)
  {
    std::ignore = val;
    return {};
  }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(uint8_t val) { return integralConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(int16_t val) { return integralConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(uint16_t val) { return integralConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(int32_t val) { return integralConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(uint32_t val) { return integralConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(int64_t val) { return integralConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(uint64_t val) { return integralConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(float32_t val) { return nativeConversion(val); }

  /// Use c++ rules for native types
  [[nodiscard]] To operator()(float64_t val) { return nativeConversion(val); }

  /// Returns "true"/"false" if converts to string and
  /// uses the c++ rules for native types otherwise
  [[nodiscard]] To operator()(bool val)
  {
    // if we want to convert to string, C++ does not
    // allow us natively, so we special-case it
    if constexpr (std::is_same_v<std::string, To>)
    {
      return (val ? std::string("true") : std::string("false"));
    }

    /// Special case for durations, as they have a non-explicit constructor
    if constexpr (std::is_same_v<Duration, To>)
    {
      throwRuntimeError("cannot convert from boolean");
      SEN_UNREACHABLE();
    }

    /// Use c++ rules for native types
    if constexpr (std::is_convertible_v<bool, To>)
    {
      return static_cast<To>(val);
    }

    throwRuntimeError("cannot convert from boolean");
    SEN_UNREACHABLE();
  }

  /// Try to convert strings to something
  [[nodiscard]] To operator()(const std::string& str)
  {
    // booleans
    if constexpr (std::is_same_v<bool, To>)
    {

      // return false for an empty string
      if (str.empty())
      {
        return false;
      }

      // here we do explicit checks for "true" and "false"
      if (str == "true")
      {
        return true;
      }

      if (str == "false")
      {
        return false;
      }
    }

    // durations
    if constexpr (std::is_same_v<Duration, To>)
    {
      return stringToDuration(str);
    }

    // timestamps
    if constexpr (std::is_same_v<TimeStamp, To>)
    {
      return TimeStamp::make(str).getValue();  // NOLINT
    }

    // integrals
    if constexpr (std::is_integral_v<To>)
    {
      if constexpr (std::is_signed_v<To>)
      {
        if constexpr (sizeof(To) == sizeof(int64_t))
        {
          return static_cast<To>(std::stoll(str));
        }

        if constexpr (sizeof(To) == sizeof(int32_t))
        {
          return static_cast<To>(std::stol(str));
        }

        return static_cast<To>(std::stoi(str));
      }
      else
      {
        if constexpr (sizeof(To) == sizeof(uint64_t))
        {
          return static_cast<To>(std::stoull(str));
        }

        if constexpr (sizeof(To) == sizeof(uint32_t))
        {
          return static_cast<To>(std::stoul(str));
        }

        return static_cast<To>(std::stoi(str));
      }
    }

    // reals
    if constexpr (std::is_same_v<To, float32_t>)
    {
      // handle empty string case to avoid exceptions
      if (str.empty())
      {
        return 0.0f;
      }

      return std::stof(str);
    }

    if constexpr (std::is_same_v<To, float64_t>)
    {
      // handle empty string case to avoid exceptions
      if (str.empty())
      {
        return 0.0;
      }

      return std::stod(str);
    }

    // enums
    if constexpr (std::is_enum_v<To>)
    {
      // handle empty string case to avoid exceptions
      if (str.empty())
      {
        return StringConversionTraits<To>::fromString(MetaTypeTrait<To>::meta().getEnumFromKey(0U).name);
      }

      return StringConversionTraits<To>::fromString(str);
    }

    throwRuntimeError("cannot convert from string");
    SEN_UNREACHABLE();
  }

  /// Durations can be converted to:
  ///   - strings: the nanosecond value
  ///   - timestamps
  ///   - any integral type that can hold nanoseconds
  [[nodiscard]] To operator()(const Duration& val)
  {
    std::ignore = val;

    if constexpr (std::is_same_v<std::string, To>)
    {
      return std::to_string(val.getNanoseconds());
    }

    if constexpr (std::is_same_v<TimeStamp, To>)
    {
      return TimeStamp(val);
    }

    if constexpr (std::is_integral_v<To>)
    {
      return static_cast<To>(val.getNanoseconds());
    }

    if constexpr (std::is_convertible_v<Duration, To>)
    {
      return static_cast<To>(val);
    }

    throwRuntimeError("cannot convert from duration");
    SEN_UNREACHABLE();
  }

  /// Timestamps can be converted to:
  ///   - strings
  ///   - durations (since epoch)
  ///   - any integral type that can hold nanoseconds
  [[nodiscard]] To operator()(const TimeStamp& val)
  {
    std::ignore = val;

    if constexpr (std::is_same_v<std::string, To>)
    {
      return val.toUtcString();
    }

    if constexpr (std::is_same_v<Duration, To>)
    {
      return Duration(val.sinceEpoch().getNanoseconds());
    }

    if constexpr (std::is_integral_v<To>)
    {
      return static_cast<To>(val.sinceEpoch().getNanoseconds());
    }

    if constexpr (std::is_convertible_v<TimeStamp, To>)
    {
      return static_cast<To>(val);
    }

    throwRuntimeError("cannot convert from timestamp");
    SEN_UNREACHABLE();
  }

  /// Lists can only be converted to strings
  [[nodiscard]] To operator()(const VarList& val)
  {
    std::ignore = val;
    if constexpr (std::is_same_v<std::string, To>)
    {
      return toJson(val);
    }

    return {};
  }

  /// Maps can only be converted to strings
  [[nodiscard]] To operator()(const VarMap& val)
  {
    std::ignore = val;
    if constexpr (std::is_same_v<std::string, To>)
    {
      return toJson(val);
    }

    return {};
  }

  /// Keyed vars are converted to maps, then converted to strings
  [[nodiscard]] To operator()(const KeyedVar& val)
  {
    std::ignore = val;
    if constexpr (std::is_same_v<std::string, To>)
    {
      VarMap map;
      map.try_emplace("key", std::get<0>(val));
      map.try_emplace("value", *std::get<1>(val));
      return toJson(map);
    }

    throwRuntimeError("cannot convert from keyed variant");
    SEN_UNREACHABLE();
  }

private:
  template <typename From>
  [[nodiscard]] To nativeConversion(From from)
  {
    std::ignore = from;

    if constexpr (std::is_same_v<std::string, To>)
    {
      return std::to_string(from);
    }

    if constexpr (std::is_convertible_v<From, To>)
    {
      if constexpr ((std::is_floating_point_v<From> || std::is_integral_v<From>) &&
                    (std::is_floating_point_v<To> || std::is_integral_v<To>))
      {
        if constexpr (checkedConversion)
        {
#if NDEBUG
          return sen::std_util::checkedConversion<To, std_util::ReportPolicyLog>(from);
#else
          return sen::std_util::checkedConversion<To, std_util::ReportPolicyAssertion>(from);
#endif
        }
        else
        {
          return sen::std_util::ignoredLossyConversion<To>(from);
        }
      }
      return static_cast<To>(from);
    }

    throwRuntimeError("cannot convert from/to native types");
    SEN_UNREACHABLE();
  }

  template <typename From>
  [[nodiscard]] To integralConversion(From from)
  {
    // timestamp
    if constexpr (std::is_same_v<TimeStamp, To>)
    {
      return TimeStamp {Duration {from}};
    }

    // duration
    if constexpr (std::is_same_v<Duration, To>)
    {
      return Duration(from);
    }

    return nativeConversion(from);
  }
};

template struct Converter<std::monostate>;
template struct Converter<int32_t>;
template struct Converter<uint32_t>;
template struct Converter<int64_t>;
template struct Converter<uint64_t>;
template struct Converter<float32_t>;
template struct Converter<float64_t>;
template struct Converter<Duration>;
template struct Converter<TimeStamp>;
template struct Converter<uint8_t>;
template struct Converter<int16_t>;
template struct Converter<uint16_t>;
template struct Converter<bool>;
template struct Converter<std::string>;
template struct Converter<VarList>;
template struct Converter<VarMap>;
template struct Converter<KeyedVar>;
template struct Converter<std::monostate, /*checkedConversion=*/true>;
template struct Converter<int32_t, /*checkedConversion=*/true>;
template struct Converter<uint32_t, /*checkedConversion=*/true>;
template struct Converter<int64_t, /*checkedConversion=*/true>;
template struct Converter<uint64_t, /*checkedConversion=*/true>;
template struct Converter<float32_t, /*checkedConversion=*/true>;
template struct Converter<float64_t, /*checkedConversion=*/true>;
template struct Converter<Duration, /*checkedConversion=*/true>;
template struct Converter<TimeStamp, /*checkedConversion=*/true>;
template struct Converter<uint8_t, /*checkedConversion=*/true>;
template struct Converter<int16_t, /*checkedConversion=*/true>;
template struct Converter<uint16_t, /*checkedConversion=*/true>;
template struct Converter<bool, /*checkedConversion=*/true>;
template struct Converter<std::string, /*checkedConversion=*/true>;
template struct Converter<VarList, /*checkedConversion=*/true>;
template struct Converter<VarMap, /*checkedConversion=*/true>;
template struct Converter<KeyedVar, /*checkedConversion=*/true>;

//--------------------------------------------------------------------------------------------------------------
// Free functions
//--------------------------------------------------------------------------------------------------------------

template <bool checkedConversion = false, typename T>
void convert(const Var& var, T& val)
{
  using PlainT = std::remove_reference_t<T>;
  val = std::visit(Converter<PlainT, checkedConversion>(), static_cast<const Var::ValueType&>(var));
}

void getCopyAsImpl(const Var& var, std::monostate& val)
{
  std::ignore = var;
  std::ignore = val;
}

void getCopyAsImpl(const Var& var, int32_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, uint32_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, int64_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, uint64_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, float32_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, float64_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, Duration& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, TimeStamp& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, uint8_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, int16_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, uint16_t& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, bool& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, std::string& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, VarList& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, VarMap& val) { convert(var, val); }

void getCopyAsImpl(const Var& var, KeyedVar& val) { convert(var, val); }

void getCheckedCopyAsImpl(const Var& var, std::monostate& val)
{
  std::ignore = var;
  std::ignore = val;
}

void getCheckedCopyAsImpl(const Var& var, int32_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, uint32_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, int64_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, uint64_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, float32_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, float64_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, Duration& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, TimeStamp& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, uint8_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, int16_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, uint16_t& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, bool& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, std::string& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, VarList& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, VarMap& val) { convert</*checkedConversion=*/true>(var, val); }

void getCheckedCopyAsImpl(const Var& var, KeyedVar& val) { convert</*checkedConversion=*/true>(var, val); }

}  // namespace impl

}  // namespace sen
