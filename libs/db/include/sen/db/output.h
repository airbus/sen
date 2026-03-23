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

/// Identifies a live object to be included in a recording or keyframe. \ingroup db
struct ObjectInfo
{
  const Object* instance;  ///< Non-owning pointer to the live object; must remain valid during the write call.
  std::string session;     ///< Session name under which the object is published.
  std::string bus;         ///< Bus name on which the object is visible.
};

/// A list of information elements about tracked objects. \ingroup db
using ObjectInfoList = std::vector<ObjectInfo>;

/// Writes a Sen recording to the filesystem asynchronously.
/// All `write` methods are non-blocking; entries are queued and flushed by a background writer.
/// Typically owned by the kernel recorder component and not used directly by application code.
/// \ingroup db
class Output
{
  SEN_MOVE_ONLY(Output)

public:
  /// Opens (or creates) the recording directory and starts the background writer thread.
  /// @param settings      Output configuration (path, indexing options, compression, etc.).
  /// @param errorHandler  Callback invoked on the background thread if a write error occurs.
  Output(OutSettings settings, std::function<void()> errorHandler);
  ~Output();

public:
  /// Returns the settings supplied at construction.
  /// @return Reference to the internal `OutSettings`; valid for the lifetime of this Output.
  [[nodiscard]] const OutSettings& getSettings() const noexcept;

  /// Returns the wall-clock time at which this recording was created.
  /// @return Reference to the creation timestamp; valid for the lifetime of this Output.
  [[nodiscard]] const TimeStamp& getCreationTime() const noexcept;

  /// Returns a snapshot of current recording statistics (bytes written, entry counts, etc.).
  /// Thread-safe; may be called concurrently with write methods.
  /// @return Copy of the current `OutStats`.
  [[nodiscard]] OutStats fetchStats() const;

public:
  /// Enqueues a keyframe entry capturing the full property state of the given objects.
  /// @param time    Simulation timestamp for this keyframe.
  /// @param objects List of live objects whose state should be snapshotted.
  void keyframe(TimeStamp time, const ObjectInfoList& objects);

  /// Enqueues an object-creation entry for the given object.
  /// @param time    Simulation timestamp of the creation event.
  /// @param object  The newly published object and its bus/session metadata.
  /// @param indexed If `true`, also adds the object to the archive index for random-access playback.
  void creation(TimeStamp time, const ObjectInfo& object, bool indexed);

  /// Enqueues an object-deletion entry.
  /// @param time      Simulation timestamp of the deletion event.
  /// @param objectId  ID of the object that was un-published.
  void deletion(TimeStamp time, ObjectId objectId);

  /// Enqueues a property-change entry.
  /// @param time          Simulation timestamp of the property update.
  /// @param objectId      ID of the object whose property changed.
  /// @param propertyId    Hash of the property descriptor (`MemberHash`).
  /// @param propertyValue Serialised new value in Sen wire format (moved in).
  void propertyChange(TimeStamp time, ObjectId objectId, MemberHash propertyId, ::sen::kernel::Buffer&& propertyValue);

  /// Enqueues an event-emission entry.
  /// @param time      Simulation timestamp of the event.
  /// @param objectId  ID of the object that emitted the event.
  /// @param eventId   Hash of the event descriptor (`MemberHash`).
  /// @param args      Serialised event arguments in Sen wire format (moved in).
  void event(TimeStamp time, ObjectId objectId, MemberHash eventId, ::sen::kernel::Buffer&& args);

  /// Enqueues a user-defined annotation entry.
  /// @param time  Simulation timestamp to associate with the annotation.
  /// @param type  Type descriptor for the annotation value.
  /// @param value Serialised annotation value in Sen wire format (moved in).
  void annotation(TimeStamp time, const Type* type, ::sen::kernel::Buffer&& value);

private:
  class Impl;

private:
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::db

#endif  // SEN_DB_OUTPUT_H
