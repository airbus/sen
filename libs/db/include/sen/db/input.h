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

/// The location of a keyframe in an archive. \ingroup db
struct KeyframeIndex
{
  uint64_t offset;
  TimeStamp time;
};

/// Describes an object that has been indexed. \ingroup db
struct ObjectIndexDef
{
  ConstTypeHandle<ClassType> type;
  ObjectId objectId;
  std::string name;
  std::string session;
  std::string bus;
  uint32_t indexId;
};

/// Represents the end of the recording. Used in cursors. \ingroup db
class End
{
};

/// A cursor that can iterate over data entries. \ingroup db
using DataCursor = Cursor<End, PropertyChange, Event, Keyframe, Creation, Deletion>;

/// A cursor that can iterate over annotation entries. \ingroup db
using AnnotationCursor = Cursor<End, Annotation>;

/// Allows you to access an existing recording in the file system. \ingroup db
class Input final
{
  SEN_NOCOPY_NOMOVE(Input)

public:
  Input(std::filesystem::path path, CustomTypeRegistry& nativeTypes);
  ~Input();

public:
  /// The folder where this archive is stored
  [[nodiscard]] const std::filesystem::path& getPath() const noexcept;

  /// Summary data of this archive
  [[nodiscard]] const Summary& getSummary() const noexcept;

  /// Types available in this archive
  [[nodiscard]] const CustomTypeRegistry& getTypes() const noexcept;

  /// Creates a cursor to the start of the runtime data.
  [[nodiscard]] DataCursor begin();

  /// Creates a cursor to the start of the annotations.
  [[nodiscard]] AnnotationCursor annotationsBegin();

  /// Creates a cursor located at a given keyframe
  [[nodiscard]] DataCursor at(const KeyframeIndex& index);

  /// Creates a cursor that only iterates over the data related to an object
  [[nodiscard]] DataCursor makeCursor(const ObjectIndexDef& indexDef);

  /// Adds an annotation. Does not change the file summary.
  void addAnnotation(TimeStamp time, const Type* type, std::vector<uint8_t>&& value);

public:
  /// The objects that have been indexed
  [[nodiscard]] Span<const ObjectIndexDef> getObjectIndexDefinitions();

  /// The keyframes that were indexed (if any)
  [[nodiscard]] Span<const KeyframeIndex> getAllKeyframeIndexes();

  /// The closest keyframe to a given time
  [[nodiscard]] std::optional<KeyframeIndex> getKeyframeIndex(TimeStamp time);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::db

#endif  // SEN_DB_INPUT_H
