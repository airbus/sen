// === time_types.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/time_types.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <optional>
#include <ostream>

namespace sen
{

std::ostream& operator<<(std::ostream& out, const Duration& val) { return out << val.toSeconds() << " s"; }

//--------------------------------------------------------------------------------------------------------------
// DurationType
//--------------------------------------------------------------------------------------------------------------

DurationType::DurationType() noexcept
  : QuantityType({"Duration",
                  "sen.Duration",
                  "a time duration",
                  Int64Type::get(),
                  &QuantityTraits<Duration>::unit(),
                  std::nullopt,
                  std::nullopt})
{
  SEN_ENSURE(getSpec().unit != nullptr);
}

bool DurationType::equals(const Type& other) const noexcept { return other.isDurationType(); }

sen::TypeHandle<const DurationType> DurationType::get()
{
  static DurationType instance;
  return makeNonOwningTypeHandle(&instance);
}

std::ostream& operator<<(std::ostream& out, const TimeStamp& val) { return out << val.toUtcString(); }

//--------------------------------------------------------------------------------------------------------------
// TimestampType
//--------------------------------------------------------------------------------------------------------------

TimestampType::TimestampType() noexcept
  : QuantityType({"TimeStamp",
                  "sen.TimeStamp",
                  "a point in time",
                  Int64Type::get(),
                  &QuantityTraits<TimeStamp>::unit(),
                  std::nullopt,
                  std::nullopt})
{
}

bool TimestampType::equals(const Type& other) const noexcept { return other.isTimestampType(); }

sen::TypeHandle<const TimestampType> TimestampType::get()
{
  static TimestampType instance;
  return makeNonOwningTypeHandle(&instance);
}

}  // namespace sen
