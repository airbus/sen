// === recording_merger.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_RECORDING_MERGER_H
#define SEN_DB_RECORDING_MERGER_H

// sen
#include "sen/core/base/duration.h"

// std
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace sen::db
{

/// Timestamp alignment modes supported when merging recording archives.  \ingroup db
enum class RecordingMergeMode
{
  normalMerge,
  zeroAligned,
  offsetAligned
};

/// Information about an object found while merging recordings. \ingroup db
class RecordingMergeObject
{
public:
  RecordingMergeObject(std::filesystem::path archivePath,
                       uint32_t objectId,
                       std::string session,
                       std::string bus,
                       std::string name,
                       std::string type);

  [[nodiscard]] const std::filesystem::path& getArchivePath() const noexcept;

  [[nodiscard]] uint32_t getObjectId() const noexcept;

  [[nodiscard]] const std::string& getSession() const noexcept;

  [[nodiscard]] const std::string& getBus() const noexcept;

  [[nodiscard]] const std::string& getName() const noexcept;

  [[nodiscard]] const std::string& getType() const noexcept;

  /// Formats the object full name.
  ///
  /// @return The object name in `session.bus.name` format.
  [[nodiscard]] std::string formatFullName() const;

private:
  std::filesystem::path archivePath_;
  uint32_t objectId_;
  std::string session_;
  std::string bus_;
  std::string name_;
  std::string type_;
};

/// Conflict data passed to the duplicate object resolver. \ingroup db
struct RecordingMergeDuplicateObject
{
  std::vector<RecordingMergeObject> objects;
};

/// New name selected for one object in a duplicate object conflict. \ingroup db
struct RecordingMergeObjectRename
{
  std::size_t objectIndex = 0U;
  std::string name;
};

/// Object that we need to keep and discards the other in a duplicate conflict. \ingroup db
struct RecordingMergeKeepSelectedObject
{
  std::size_t selectedObjectIndex = 0U;
};

/// Resolution that assigns new names to objects in a duplicate conflict. \ingroup db
struct RecordingMergeRenameObjects
{
  std::vector<RecordingMergeObjectRename> renamedObjects;
};

/// Decision returned by the duplicate object resolver. \ingroup db
using RecordingMergeDuplicateObjectResolution =
  std::variant<RecordingMergeKeepSelectedObject, RecordingMergeRenameObjects>;

/// Callback used to resolve duplicate object names. \ingroup db
using RecordingMergeDuplicateObjectResolver =
  std::function<RecordingMergeDuplicateObjectResolution(const RecordingMergeDuplicateObject& duplicateObject)>;

/// Recording input used by the merger. \ingroup db
struct RecordingMergeInput
{
  std::filesystem::path archivePath;
  Duration offset {};
};

/// Settings used to merge several recording archives into one archive. \ingroup db
struct RecordingMergeSettings
{
  std::vector<RecordingMergeInput> inputArchives;
  std::filesystem::path outputArchive;
  RecordingMergeMode mode = RecordingMergeMode::normalMerge;
  RecordingMergeDuplicateObjectResolver duplicateObjectResolver;
};

/// Merges recording archives into a single archive.
///
/// @param settings Input archives, output archive, alignment mode, and duplicate resolution callback.
void mergeRecordings(const RecordingMergeSettings& settings);

}  // namespace sen::db

#endif  // SEN_DB_RECORDING_MERGER_H
