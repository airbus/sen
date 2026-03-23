// === input.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_INPUT_H
#define SEN_DB_INPUT_H

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/db/annotation.h"
#include "sen/db/creation.h"
#include "sen/db/cursor.h"
#include "sen/db/deletion.h"
#include "sen/db/event.h"
#include "sen/db/keyframe.h"
#include "sen/db/property_change.h"

// stl
#include "stl/sen/db/basic_types.stl.h"

// std
#include <filesystem>

namespace sen::db
{

/// Points to the byte location and timestamp of a keyframe within an archive file.
/// Obtained from `Input::getAllKeyframeIndexes()` or `Input::getKeyframeIndex()`
/// and passed to `Input::at()` to open a cursor from that keyframe. \ingroup db
struct KeyframeIndex
{
  uint64_t offset;  ///< Byte offset of the keyframe within the archive file.
  TimeStamp time;   ///< Timestamp recorded for this keyframe.
};

/// Metadata describing a single object that has been indexed in the archive. \ingroup db
struct ObjectIndexDef
{
  ConstTypeHandle<ClassType> type;  ///< Compile-time type handle for the object's class.
  ObjectId objectId;                ///< Unique identifier of the object within the recording.
  std::string name;                 ///< Human-readable name of the object instance.
  std::string session;              ///< Session name under which the object was published.
  std::string bus;                  ///< Bus name on which the object was visible.
  uint32_t indexId;                 ///< Ordinal index assigned to this object in the archive index.
};

/// Represents the end of the recording. Used in cursors. \ingroup db
class End
{
};

/// A cursor that can iterate over data entries. \ingroup db
using DataCursor = Cursor<End, PropertyChange, Event, Keyframe, Creation, Deletion>;

/// A cursor that can iterate over annotation entries. \ingroup db
using AnnotationCursor = Cursor<End, Annotation>;

/// Read-only accessor for an existing Sen recording stored on the filesystem.
/// Opens the archive, deserialises the index, and provides cursors for sequential playback.
/// \ingroup db
class Input final
{
  SEN_NOCOPY_NOMOVE(Input)

public:
  /// Opens the recording at the given path and registers its custom types.
  /// @param path        Path to the recording directory produced by `Output`.
  /// @param nativeTypes Type registry used to resolve class descriptors referenced by the archive.
  Input(std::filesystem::path path, CustomTypeRegistry& nativeTypes);
  ~Input();

public:
  /// Returns the directory path of this recording archive.
  /// @return Reference to the path passed at construction; valid for the lifetime of this Input.
  [[nodiscard]] const std::filesystem::path& getPath() const noexcept;

  /// Returns the summary metadata stored in the archive (duration, object count, etc.).
  /// @return Reference to the archive's `Summary`; valid for the lifetime of this Input.
  [[nodiscard]] const Summary& getSummary() const noexcept;

  /// Returns the type registry built from the types declared in this archive.
  /// @return Reference to the internal `CustomTypeRegistry`; valid for the lifetime of this Input.
  [[nodiscard]] const CustomTypeRegistry& getTypes() const noexcept;

  /// Creates a `DataCursor` positioned before the first entry in the recording.
  /// Advance with `++cursor` to start reading entries.
  /// @return A new cursor over the full data stream (`PropertyChange`, `Event`, `Keyframe`, etc.).
  [[nodiscard]] DataCursor begin();

  /// Creates an `AnnotationCursor` positioned before the first annotation entry.
  /// @return A new cursor over the annotation stream.
  [[nodiscard]] AnnotationCursor annotationsBegin();

  /// Creates a `DataCursor` positioned at the given keyframe.
  /// Useful for seeking to a known point in the recording without replaying from the start.
  /// @param index A `KeyframeIndex` obtained from `getAllKeyframeIndexes()` or `getKeyframeIndex()`.
  /// @return A new cursor starting from the specified keyframe position.
  [[nodiscard]] DataCursor at(const KeyframeIndex& index);

  /// Creates a `DataCursor` that yields only entries related to a specific indexed object.
  /// @param indexDef An `ObjectIndexDef` from `getObjectIndexDefinitions()`.
  /// @return A new cursor filtered to entries for the given object.
  [[nodiscard]] DataCursor makeCursor(const ObjectIndexDef& indexDef);

  /// Appends an annotation entry to the recording.
  /// The file summary is not updated; call this before closing the archive for consistency.
  /// @param time  Timestamp to associate with the annotation.
  /// @param type  Type descriptor for the annotation value.
  /// @param value Serialised annotation value in Sen wire format (moved in).
  void addAnnotation(TimeStamp time, const Type* type, std::vector<uint8_t>&& value);

public:
  /// Returns the list of all objects that have been indexed in this archive.
  /// @return Non-owning span of `ObjectIndexDef` entries; valid for the lifetime of this Input.
  [[nodiscard]] Span<const ObjectIndexDef> getObjectIndexDefinitions();

  /// Returns all keyframe index entries stored in the archive (may be empty).
  /// @return Non-owning span of `KeyframeIndex` entries sorted by time; valid for the lifetime of this Input.
  [[nodiscard]] Span<const KeyframeIndex> getAllKeyframeIndexes();

  /// Returns the keyframe index entry closest to (but not after) the given time.
  /// @param time The target playback time.
  /// @return The nearest `KeyframeIndex`, or `std::nullopt` if no keyframes exist.
  [[nodiscard]] std::optional<KeyframeIndex> getKeyframeIndex(TimeStamp time);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::db

#endif  // SEN_DB_INPUT_H
