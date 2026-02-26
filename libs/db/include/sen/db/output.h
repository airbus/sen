// === output.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_OUTPUT_H
#define SEN_DB_OUTPUT_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <functional>

namespace sen::db
{

/// A function to be called in case of error. \ingroup db
using ErrorHandler = std::function<void()>;

/// Basic information about an object tracked for recording. \ingroup db
struct ObjectInfo
{
  const Object* instance;
  std::string session;
  std::string bus;
};

/// A list of information elements about tracked objects. \ingroup db
using ObjectInfoList = std::vector<ObjectInfo>;

/// Allows the creation of recordings. \ingroup db
class Output
{
  SEN_MOVE_ONLY(Output)

public:
  /// Takes the settings and a function to call in case of error.
  Output(OutSettings settings, std::function<void()> errorHandler);
  ~Output();

public:
  /// The settings passed during construction.
  [[nodiscard]] const OutSettings& getSettings() const noexcept;

  /// The time of creation of the recording.
  [[nodiscard]] const TimeStamp& getCreationTime() const noexcept;

  /// Fetch the current statistics. Thread-safe.
  [[nodiscard]] OutStats fetchStats() const;

public:
  /// Asynchronously write a keyframe containing the state of the given objects.
  void keyframe(TimeStamp time, const ObjectInfoList& objects);

  /// Asynchronously write a creation entry for the given object. Index it if needed.
  void creation(TimeStamp time, const ObjectInfo& object, bool indexed);

  /// Asynchronously write a deletion entry for the given object.
  void deletion(TimeStamp time, ObjectId objectId);

  /// Asynchronously write a property change entry for the given object.
  void propertyChange(TimeStamp time, ObjectId objectId, MemberHash propertyId, ::sen::kernel::Buffer&& propertyValue);

  /// Asynchronously write an event entry for the given object.
  void event(TimeStamp time, ObjectId objectId, MemberHash eventId, ::sen::kernel::Buffer&& args);

  /// Asynchronously write an annotation entry.
  void annotation(TimeStamp time, const Type* type, ::sen::kernel::Buffer&& value);

private:
  class Impl;

private:
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::db

#endif  // SEN_DB_OUTPUT_H
