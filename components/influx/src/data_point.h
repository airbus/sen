// === data_point.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_DATA_POINT_H
#define SEN_COMPONENTS_INFLUX_SRC_DATA_POINT_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// std
#include <deque>
#include <string>
#include <variant>

namespace sen::components::influx
{

/// Represents a data point in InfluxDb's conceptual frame
class DataPoint
{
  SEN_COPY_MOVE(DataPoint)

public:
  using FieldValue = std::variant<int32_t,
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
                                  std::string>;

  using FieldSet = std::deque<std::pair<std::string, FieldValue>>;
  using TagSet = std::deque<std::pair<std::string, std::string>>;

public:
  /// Constructs point based on measurement name
  DataPoint(std::string measurement, TimeStamp timeStamp)
    : measurement_(std::move(measurement)), timestamp_(timeStamp), tags_({}), fields_({})
  {
  }
  ~DataPoint() = default;

public:
  /// Adds a tags
  DataPoint& addTag(std::string_view key, std::string_view value);

  /// Adds field
  DataPoint& addField(std::string_view name, const FieldValue& value);

  /// Name getter
  [[nodiscard]] const std::string& getName() const noexcept { return measurement_; }

  /// Timestamp getter
  [[nodiscard]] TimeStamp getTimestamp() const noexcept { return timestamp_; }

  /// Get Field Set
  [[nodiscard]] const FieldSet& getFieldSet() const noexcept { return fields_; }

  /// Get Tag Set
  [[nodiscard]] const TagSet& getTagSet() const noexcept { return tags_; }

private:
  std::string measurement_;
  TimeStamp timestamp_;
  TagSet tags_;
  FieldSet fields_;
};

void varToFields(DataPoint& point,
                 const sen::Type* type,
                 const sen::Var& value,
                 sen::TimeStamp time,
                 const std::string& fieldName);

inline DataPoint& DataPoint::addTag(std::string_view key, std::string_view value)
{
  if (key.empty() || value.empty())
  {
    return *this;
  }

  tags_.emplace_back(key, value);
  return *this;
}

inline DataPoint& DataPoint::addField(std::string_view name, const FieldValue& value)
{
  if (name.empty())
  {
    return *this;
  }

  fields_.emplace_back(name, value);
  return *this;
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_DATA_POINT_H
