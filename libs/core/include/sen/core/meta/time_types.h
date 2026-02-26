// === time_types.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_TIME_TYPES_H
#define SEN_CORE_META_TIME_TYPES_H

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/basic_traits.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"

namespace sen
{

/// \addtogroup types
/// @{

/// Represents a sen::Duration (a time duration).
class DurationType final: public QuantityType
{
  SEN_META_TYPE(DurationType)

public:
  using ValueType = Duration;

public:  // Special members.
  DurationType() noexcept;
  ~DurationType() override = default;

public:
  [[nodiscard]] bool equals(const Type& other) const noexcept override;

public:
  /// Unique instance
  [[nodiscard]] static sen::TypeHandle<const DurationType> get();
};

/// Represents a sen::TimeStamp (a point in time).
class TimestampType final: public QuantityType
{
  SEN_META_TYPE(TimestampType)

public:
  using ValueType = TimeStamp;

public:  // Special members.
  TimestampType() noexcept;
  ~TimestampType() override = default;

public:
  [[nodiscard]] bool equals(const Type& other) const noexcept override;

public:
  /// Unique instance
  [[nodiscard]] static sen::TypeHandle<const TimestampType> get();
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// DurationType traits
//--------------------------------------------------------------------------------------------------------------

/// \ingroup traits
template <>
struct MetaTypeTrait<Duration>
{
  [[nodiscard]] static ConstTypeHandle<DurationType> meta() { return DurationType::get(); }
};

template <>
struct QuantityTraits<Duration>
{
  static constexpr bool available = true;

  [[nodiscard]] static const Unit& unit()
  {
    static const auto* unit = UnitRegistry::get().searchUnitByAbbreviation("ns").value();
    return *unit;
  }
};

template <>
struct VariantTraits<Duration>
{
  static void valueToVariant(Duration val, Var& var) { var = val; }

  static void variantToValue(const Var& var, Duration& val)
  {
    // try to convert from string, maybe with custom unit
    if (var.holds<std::string>())
    {
      auto result = QuantityTraits<Duration>::unit().fromString(var.get<std::string>());
      if (result.isOk())
      {
        val = Duration(result.getValue());
        return;
      }
    }

    val = var.getCopyAs<Duration>();
  }
};

template <>
struct SerializationTraits<Duration>
{
  static void write(OutputStream& out, Duration val) { out.writeInt64(val.get()); }

  static void read(InputStream& in, Duration& val)
  {
    int64_t nanoseconds;
    in.readInt64(nanoseconds);

    val.set(nanoseconds);
  }

  [[nodiscard]] static constexpr uint32_t serializedSize(Duration val) noexcept
  {
    return impl::getSerializedSize(val.get());
  }
};

std::ostream& operator<<(std::ostream& out, const Duration& val);

//--------------------------------------------------------------------------------------------------------------
// TimeStamp traits
//--------------------------------------------------------------------------------------------------------------

/// \ingroup traits
template <>
struct MetaTypeTrait<TimeStamp>
{
  [[nodiscard]] static ConstTypeHandle<TimestampType> meta() { return TimestampType::get(); }
};

template <>
struct VariantTraits<TimeStamp>
{
  static void valueToVariant(TimeStamp val, Var& var) { var = val; }

  static void variantToValue(const Var& var, TimeStamp& val) { val = var.getCopyAs<TimeStamp>(); }
};

template <>
struct SerializationTraits<TimeStamp>
{
  static void write(OutputStream& out, TimeStamp val) { out.writeTimestamp(val); }

  static void read(InputStream& in, TimeStamp& val) { in.readTimeStamp(val); }

  [[nodiscard]] static constexpr uint32_t serializedSize(TimeStamp val) noexcept
  {
    return impl::getSerializedSize(val);
  }
};

template <>
class QuantityTraits<TimeStamp>
{
public:
  static constexpr bool available = true;

  [[nodiscard]] static const Unit& unit()
  {
    static const auto* unit = UnitRegistry::get().searchUnitByAbbreviation("ns").value();
    return *unit;
  }
};

std::ostream& operator<<(std::ostream& out, const TimeStamp& val);

}  // namespace sen

#endif  // SEN_CORE_META_TIME_TYPES_H
