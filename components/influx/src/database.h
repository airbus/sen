// === database.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_DATABASE_H
#define SEN_COMPONENTS_INFLUX_SRC_DATABASE_H

#include "data_point.h"

namespace sen::components::influx
{

class Transport
{
  SEN_MOVE_ONLY(Transport)

public:
  Transport() = default;
  virtual ~Transport() = default;

public:
  virtual void send(std::string&& message) = 0;
};

class Database
{
  SEN_MOVE_ONLY(Database)

public:
  /// Constructor required valid transport
  explicit Database(std::unique_ptr<Transport> transport): transport_(std::move(transport))
  {
    SEN_ASSERT(transport_.get() != nullptr);
  }
  ~Database() = default;

public:
  /// Writes a point
  void write(DataPoint&& point);

  /// Flushes points batched (this can also happens when buffer is full)
  void flushBatch();

  /// Enables points batching
  void batchOf(std::size_t size = 32)
  {
    batchSize_ = size;
    isBatchingActivated_ = true;
  }

private:
  enum class ElementType
  {
    measurement,
    tagKey,
    tagValue,
    fieldKey,
    fieldValue
  };

private:
  void transmit(std::string&& point);
  [[nodiscard]] std::string joinLineProtocolBatch() const;
  void addPointToBatch(DataPoint&& point);
  [[nodiscard]] std::string makeLine(const DataPoint& point) const;
  static void appendIfNotEmpty(std::string& dest, std::string_view value, char separator);
  [[nodiscard]] static std::string escapeCharacters(std::string_view input, std::string_view escapedChars);
  [[nodiscard]] static std::string escapeStringElement(ElementType type, std::string_view element);
  [[nodiscard]] static std::string formatFields(const DataPoint& point);
  [[nodiscard]] static std::string formatTags(const DataPoint& point);

private:
  std::unique_ptr<Transport> transport_;
  std::deque<DataPoint> pointBatch_;
  bool isBatchingActivated_ = false;
  std::size_t batchSize_ = 0;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline void Database::write(DataPoint&& point)
{
  if (isBatchingActivated_)
  {
    addPointToBatch(std::move(point));
  }
  else
  {
    transmit(makeLine(point));
  }
}

inline void Database::flushBatch()
{
  if (isBatchingActivated_ && !pointBatch_.empty())
  {
    transmit(joinLineProtocolBatch());
    pointBatch_.clear();
  }
}

inline void Database::transmit(std::string&& point) { transport_->send(std::move(point)); }

inline void Database::addPointToBatch(DataPoint&& point)
{
  pointBatch_.emplace_back(std::move(point));

  if (pointBatch_.size() >= batchSize_)
  {
    flushBatch();
  }
}

inline void Database::appendIfNotEmpty(std::string& dest, std::string_view value, char separator)
{
  if (!value.empty())
  {
    dest.append(std::string {separator}).append(value);
  }
}

inline std::string Database::makeLine(const DataPoint& point) const
{
  std::string line {escapeStringElement(ElementType::measurement, point.getName())};
  appendIfNotEmpty(line, formatTags(point), ',');
  appendIfNotEmpty(line, formatFields(point), ' ');

  return line.append(" ").append(std::to_string(point.getTimestamp().sinceEpoch().getNanoseconds()));
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_DATABASE_H
