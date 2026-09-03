// === recording_merger.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/recording_merger.h"

// implementation
#include "constants.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type_traits.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/type_specs.stl.h"
#include "v1.stl.h"

// std
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS  // NOLINT
#  pragma warning(disable: 4996)   // NOLINT
#endif

namespace sen::db
{

RecordingMergeObject::RecordingMergeObject(std::filesystem::path archivePath,
                                           uint32_t objectId,
                                           std::string session,
                                           std::string bus,
                                           std::string name,
                                           std::string type)
  : archivePath_(std::move(archivePath))
  , objectId_(objectId)
  , session_(std::move(session))
  , bus_(std::move(bus))
  , name_(std::move(name))
  , type_(std::move(type))
{
}

const std::filesystem::path& RecordingMergeObject::getArchivePath() const noexcept { return archivePath_; }

uint32_t RecordingMergeObject::getObjectId() const noexcept { return objectId_; }

const std::string& RecordingMergeObject::getSession() const noexcept { return session_; }

const std::string& RecordingMergeObject::getBus() const noexcept { return bus_; }

const std::string& RecordingMergeObject::getName() const noexcept { return name_; }

const std::string& RecordingMergeObject::getType() const noexcept { return type_; }

std::string RecordingMergeObject::formatFullName() const
{
  std::string fullName;

  fullName.reserve(session_.size() + bus_.size() + name_.size() + 2U);
  fullName.append(session_);
  fullName.append(".");
  fullName.append(bus_);
  fullName.append(".");
  fullName.append(name_);

  return fullName;
}

namespace
{

struct FileCloser
{
  void operator()(FILE* file) const noexcept
  {
    if (file != nullptr)
    {
      std::ignore = fclose(file);  // NOLINT
    }
  }
};

using FilePtr = std::unique_ptr<FILE, FileCloser>;

struct MergedAnnotationEntry
{
  TimeStamp time;
  std::vector<uint8_t> bytes;
};

struct InputArchivePlan
{
  std::filesystem::path archivePath;
  Duration timeShift {};
};

struct ObjectNameKey
{
  std::string session;
  std::string bus;
  std::string name;
};

[[nodiscard]] bool operator==(const ObjectNameKey& lhs, const ObjectNameKey& rhs)
{
  return lhs.session == rhs.session && lhs.bus == rhs.bus && lhs.name == rhs.name;
}

struct ObjectNameKeyHash
{
  [[nodiscard]] std::size_t operator()(const ObjectNameKey& key) const
  {
    const auto sessionHash = std::hash<std::string> {}(key.session);
    const auto busHash = std::hash<std::string> {}(key.bus);
    const auto nameHash = std::hash<std::string> {}(key.name);
    return sessionHash ^ (busHash << 1U) ^ (nameHash << 2U);
  }
};

/// Tracks merged objects and keep-or-rename decisions
class MergedObjectRegistry
{
public:
  void registerObject(RecordingMergeObject object);
  [[nodiscard]] bool contains(uint32_t objectId) const;
  [[nodiscard]] bool isIgnored(uint32_t objectId) const;
  [[nodiscard]] const std::string* findRenamedName(uint32_t objectId) const;
  [[nodiscard]] bool hasIgnoredObjects() const noexcept;
  [[nodiscard]] bool hasRenamedObjects() const noexcept;
  void resolveDuplicateObjects(const RecordingMergeDuplicateObjectResolver& resolver);

private:
  void ignoreObject(uint32_t objectId);
  void renameObject(uint32_t objectId, std::string name);
  void applyDuplicateObjectResolution(const RecordingMergeDuplicateObject& duplicateObject,
                                      const RecordingMergeDuplicateObjectResolution& resolution);
  void validateResolvedObjectNames() const;

private:
  std::unordered_map<ObjectNameKey, std::vector<uint32_t>, ObjectNameKeyHash> objectIdsByName_;
  std::unordered_map<uint32_t, RecordingMergeObject> objectById_;
  std::unordered_set<uint32_t> ignoredObjectIds_;
  std::unordered_map<uint32_t, std::string> renamedObjectNames_;
};

/// Index definitions and lookup tables
class IndexedObjectRegistry
{
public:
  void addDefinition(v1::IndexDefinition definition, const std::filesystem::path& archivePath);
  void removeIgnoredDefinitions(const MergedObjectRegistry& objectRegistry);
  void applyObjectRenames(const MergedObjectRegistry& objectRegistry);
  [[nodiscard]] std::optional<uint32_t> findIndexId(uint32_t objectId) const;
  [[nodiscard]] const std::vector<v1::IndexDefinition>& getDefinitions() const noexcept;

private:
  std::vector<v1::IndexDefinition> definitions_;
  std::unordered_map<uint32_t, std::size_t> definitionPositionByIndexId_;
  std::unordered_map<uint32_t, uint32_t> indexIdByObjectId_;
};

/// Maps the object IDs of one input archive to IDs that are unique
class ObjectIdMapper
{
public:
  explicit ObjectIdMapper(uint32_t& nextObjectId): nextObjectId_(nextObjectId) {}

  [[nodiscard]] uint32_t map(uint32_t objectId)
  {
    const auto position = mappedObjectIds_.find(objectId);
    if (position != mappedObjectIds_.end())
    {
      return position->second;
    }

    if (nextObjectId_ == keyframeIndexId)
    {
      ++nextObjectId_;
    }
    if (nextObjectId_ == std::numeric_limits<uint32_t>::max())
    {
      throwRuntimeError("could not allocate a new object id while merging recordings");
    }

    const auto mappedObjectId = nextObjectId_;
    ++nextObjectId_;
    mappedObjectIds_.emplace(objectId, mappedObjectId);
    return mappedObjectId;
  }

private:
  std::unordered_map<uint32_t, uint32_t> mappedObjectIds_;
  uint32_t& nextObjectId_;
};

class ObjectUseTimeRange;

/// Stores a runtime entry with the metadata needed for filtering, sorting and indexing
struct PreparedRuntimeEntry
{
  TimeStamp time;
  std::optional<uint32_t> objectId;
  std::vector<uint32_t> keyframeObjectIds;
};

class MergedRuntimeEntry
{
public:
  MergedRuntimeEntry(v1::RuntimeDataEntry data, PreparedRuntimeEntry prepared);

  [[nodiscard]] const TimeStamp& getTime() const noexcept;
  [[nodiscard]] bool isKeyframe() const noexcept;
  [[nodiscard]] const std::optional<uint32_t>& getObjectId() const noexcept;
  [[nodiscard]] const v1::RuntimeDataEntry& getData() const noexcept;

  [[nodiscard]] bool removeIgnoredObjects(const MergedObjectRegistry& objectRegistry);
  void applyObjectRenames(const MergedObjectRegistry& objectRegistry);
  void updateObjectUseTimeRange(ObjectUseTimeRange& objectUseTimeRange) const;
  [[nodiscard]] TimeStamp getSortTime(const ObjectUseTimeRange& objectUseTimeRange) const;
  [[nodiscard]] uint32_t getSortPriority() const;

private:
  TimeStamp time_;
  // Sort keys are computed once per entry; deriving them inside the comparator visited the variant
  // and hit two hash maps on both sides of every comparison.
  TimeStamp sortTime_;
  uint32_t sortPriority_ = 0U;
  bool isKeyframe_ = false;
  std::optional<uint32_t> objectId_;
  // Collected while the keyframe was decompressed for other reasons, so sorting never unpacks it again.
  std::vector<uint32_t> keyframeObjectIds_;
  v1::RuntimeDataEntry data_;

public:
  void cacheSortKey(const ObjectUseTimeRange& objectUseTimeRange);
  [[nodiscard]] const TimeStamp& getCachedSortTime() const noexcept;
  [[nodiscard]] uint32_t getCachedSortPriority() const noexcept;
};

class MergedRecording;
class ProgressTracker;

class RecordingArchiveReader
{
public:
  explicit RecordingArchiveReader(InputArchivePlan inputPlan): inputPlan_(std::move(inputPlan)) {}

  void readInto(MergedRecording& mergedRecording, ProgressTracker& progressTracker) const;

private:
  [[nodiscard]] std::vector<MergedRuntimeEntry> readRuntimeEntries(ObjectIdMapper& objectIdMapper,
                                                                   MergedObjectRegistry& objectRegistry,
                                                                   ProgressTracker& progressTracker) const;
  [[nodiscard]] std::vector<MergedAnnotationEntry> readAnnotationEntries(ProgressTracker& progressTracker) const;
  void readIndexDefinitions(IndexedObjectRegistry& indexedObjects, ObjectIdMapper& objectIdMapper) const;

private:
  InputArchivePlan inputPlan_;
};

[[nodiscard]] FilePtr createFile(const std::filesystem::path& path)
{
  auto* file = fopen(path.string().c_str(), "wb");  // NOLINT
  if (file == nullptr)
  {
    std::string err;
    err.append("could not create file '");
    err.append(path.string());
    err.append("'. ");
    err.append(strerror(errno));
    throwRuntimeError(err);
  }
  return FilePtr(file);
}

void writeArchiveHeader(FILE* file, v1::FileKind kind)
{
  write<v1::Magic>({beginMagic}, file);
  write<v1::FileHeader>({1U, kind}, file);
}

[[nodiscard]] bool hasSameDefinition(const v1::IndexDefinition& lhs, const v1::IndexDefinition& rhs)
{
  return lhs.objectId == rhs.objectId && lhs.indexId == rhs.indexId && lhs.session == rhs.session &&
         lhs.bus == rhs.bus && lhs.objectName == rhs.objectName && lhs.objectType == rhs.objectType;
}

/// Accepts identical definitions but rejects conflicting index to object mappings
void IndexedObjectRegistry::addDefinition(v1::IndexDefinition definition, const std::filesystem::path& archivePath)
{
  if (definition.indexId == keyframeIndexId)
  {
    throwRuntimeError("indexed object '" + definition.objectName + "' in '" + archivePath.string() +
                      "' uses the reserved keyframe index id");
  }

  const auto position = definitionPositionByIndexId_.find(definition.indexId);
  if (position != definitionPositionByIndexId_.end())
  {
    if (!hasSameDefinition(definitions_[position->second], definition))
    {
      throwRuntimeError("conflicting indexed object definition for index id " + std::to_string(definition.indexId) +
                        " while merging '" + archivePath.string() + "'");
    }
    return;
  }

  const auto objectPosition = indexIdByObjectId_.find(definition.objectId);
  if (objectPosition != indexIdByObjectId_.end() && objectPosition->second != definition.indexId)
  {
    throwRuntimeError("conflicting indexed object definition for object id " + std::to_string(definition.objectId) +
                      " while merging '" + archivePath.string() + "'");
  }

  definitionPositionByIndexId_.emplace(definition.indexId, definitions_.size());
  indexIdByObjectId_.emplace(definition.objectId, definition.indexId);
  definitions_.push_back(std::move(definition));
}

/// Counts input bytes consumed and calls the reporter only when the whole percent changes
class ProgressTracker
{
public:
  ProgressTracker(RecordingMergeProgressReporter reporter, std::size_t totalBytes)
    : reporter_(std::move(reporter)), totalBytes_(totalBytes)
  {
    report();
  }

  void advance(std::size_t bytes)
  {
    doneBytes_ += bytes;
    report();
  }

  void finish()
  {
    doneBytes_ = totalBytes_;
    report();
  }

private:
  void report()
  {
    if (!reporter_)
    {
      return;
    }

    const auto clamped = std::min(doneBytes_, totalBytes_);
    const auto percent = totalBytes_ == 0U ? 100U : static_cast<uint32_t>((clamped * 100U) / totalBytes_);
    if (reportedPercent_.has_value() && reportedPercent_.value() == percent)
    {
      return;
    }

    reportedPercent_ = percent;
    reporter_(clamped, totalBytes_);
  }

  RecordingMergeProgressReporter reporter_;
  std::size_t totalBytes_ = 0U;
  std::size_t doneBytes_ = 0U;
  std::optional<uint32_t> reportedPercent_;
};

[[nodiscard]] std::size_t fileSizeOrZero(const std::filesystem::path& path)
{
  std::error_code errorCode;
  const auto size = std::filesystem::file_size(path, errorCode);
  return errorCode ? 0U : static_cast<std::size_t>(size);
}

[[nodiscard]] std::size_t getArchiveFileSize(FILE* file, const std::filesystem::path& path)
{
  if (fseek(file, 0, SEEK_END) != 0)
  {
    throwRuntimeError("could not seek to end of file '" + path.string() + "'");
  }

  const auto fileSize = ftell(file);
  if (fileSize < 0)
  {
    throwRuntimeError("could not determine size of file '" + path.string() + "'");
  }

  return static_cast<std::size_t>(fileSize);
}

[[nodiscard]] std::size_t getCurrentFileOffset(FILE* file, const std::filesystem::path& path)
{
  const auto offset = ftell(file);
  if (offset < 0)
  {
    throwRuntimeError("could not determine current offset in file '" + path.string() + "'");
  }

  return static_cast<std::size_t>(offset);
}

void seekArchivePayload(FILE* file, const std::filesystem::path& path)
{
  if (fseek(file, static_cast<long>(getFileHeaderSize()), SEEK_SET) != 0)  // NOLINT
  {
    throwRuntimeError("could not seek past archive header in '" + path.string() + "'");
  }
}

[[nodiscard]] std::vector<uint8_t> readFileTail(const std::filesystem::path& path)
{
  FilePtr file(openFile(path));
  const auto fileSize = getArchiveFileSize(file.get(), path);
  if (fileSize < getFileHeaderSize())
  {
    throwRuntimeError("file '" + path.string() + "' is shorter than the archive header");
  }

  seekArchivePayload(file.get(), path);

  std::vector<uint8_t> result(fileSize - getFileHeaderSize());
  if (!result.empty() && fread(result.data(), 1U, result.size(), file.get()) != result.size())
  {
    throwRuntimeError("could not read file tail from '" + path.string() + "'");
  }
  return result;
}

template <typename T>
[[nodiscard]] std::vector<uint8_t> writeSizedEntryToBytes(const T& data)
{
  const auto block = writeSizeAndDataToBuffer(data);
  const auto bytes = block->getConstSpan();
  return {bytes.begin(), bytes.end()};
}

[[nodiscard]] Summary readArchiveSummary(const std::filesystem::path& archivePath)
{
  const auto summaryBuffer = readFileTail(archivePath / summaryFileName);
  InputStream in(summaryBuffer);

  Summary summary;
  SerializationTraits<Summary>::read(in, summary);
  return summary;
}

template <typename TypeSpec>
[[nodiscard]] std::optional<uint32_t> tryCountTypeSpecs(const std::vector<uint8_t>& buffer)
{
  InputStream in(buffer);
  uint32_t count = 0U;

  try
  {
    while (!in.atEnd())
    {
      TypeSpec spec;
      SerializationTraits<TypeSpec>::read(in, spec);
      ++count;
    }
  }
  catch (...)
  {
    return std::nullopt;
  }

  return count;
}

[[nodiscard]] uint32_t countTypeSpecs(const std::vector<uint8_t>& buffer, const std::filesystem::path& archivePath)
{
  if (auto count = tryCountTypeSpecs<kernel::CustomTypeSpec>(buffer))
  {
    return count.value();
  }

  if (auto count = tryCountTypeSpecs<kernel::CustomTypeSpecV5>(buffer))
  {
    return count.value();
  }

  if (auto count = tryCountTypeSpecs<kernel::CustomTypeSpecV4>(buffer))
  {
    return count.value();
  }

  throwRuntimeError("could not count type specs from '" + archivePath.string() + "'");
}

[[nodiscard]] v1::Keyframe readKeyframe(const v1::CompressedKeyframe& entry)
{
  std::vector<uint8_t> buffer;
  uncompressBuffer(entry.buffer.asVector(), buffer, entry.decompressedSize);

  v1::Keyframe keyframe;
  InputStream in(buffer);
  SerializationTraits<v1::Keyframe>::read(in, keyframe);
  return keyframe;
}

void writeCompressedKeyframe(const v1::Keyframe& keyframe, v1::CompressedKeyframe& entry)
{
  std::vector<uint8_t> buffer;
  writeToBuffer(keyframe, buffer);

  entry.decompressedSize = static_cast<uint32_t>(buffer.size());
  writeToCompressedBuffer(buffer, entry.buffer);
}

[[nodiscard]] TimeStamp readEntryTime(const v1::RuntimeDataEntry& data)
{
  return std::visit(Overloaded {[](const v1::PropertyChange& entry) { return entry.time; },
                                [](const v1::Event& entry) { return entry.time; },
                                [](const v1::Keyframe& entry) { return entry.time; },
                                [](const v1::CompressedKeyframe& entry) { return readKeyframe(entry).time; },
                                [](const v1::Creation& entry) { return entry.time; },
                                [](const v1::Deletion& entry) { return entry.time; }},
                    data);
}

void shiftCompressedKeyframe(v1::CompressedKeyframe& entry, const Duration& timeShift)
{
  auto keyframe = readKeyframe(entry);
  keyframe.time += timeShift;
  writeCompressedKeyframe(keyframe, entry);
}

void shiftRuntimeData(v1::RuntimeDataEntry& data, const Duration& timeShift)
{
  std::visit(Overloaded {[&timeShift](v1::PropertyChange& entry) { entry.time += timeShift; },
                         [&timeShift](v1::Event& entry) { entry.time += timeShift; },
                         [&timeShift](v1::Keyframe& entry) { entry.time += timeShift; },
                         [&timeShift](v1::CompressedKeyframe& entry) { shiftCompressedKeyframe(entry, timeShift); },
                         [&timeShift](v1::Creation& entry) { entry.time += timeShift; },
                         [&timeShift](v1::Deletion& entry) { entry.time += timeShift; }},
             data);
}

void remapObjectSnapshot(v1::ObjectSnapshot& snapshot, ObjectIdMapper& objectIdMapper)
{
  snapshot.objectId = objectIdMapper.map(snapshot.objectId);
}

void remapKeyframe(v1::Keyframe& keyframe, ObjectIdMapper& objectIdMapper)
{
  for (auto& snapshot: keyframe.objects)
  {
    remapObjectSnapshot(snapshot, objectIdMapper);
  }
}

void remapCompressedKeyframe(v1::CompressedKeyframe& entry, ObjectIdMapper& objectIdMapper)
{
  auto keyframe = readKeyframe(entry);
  remapKeyframe(keyframe, objectIdMapper);
  writeCompressedKeyframe(keyframe, entry);
}

void remapRuntimeData(v1::RuntimeDataEntry& data, ObjectIdMapper& objectIdMapper)
{
  std::visit(
    Overloaded {[&objectIdMapper](v1::PropertyChange& entry) { entry.objectId = objectIdMapper.map(entry.objectId); },
                [&objectIdMapper](v1::Event& entry) { entry.objectId = objectIdMapper.map(entry.objectId); },
                [&objectIdMapper](v1::Keyframe& entry) { remapKeyframe(entry, objectIdMapper); },
                [&objectIdMapper](v1::CompressedKeyframe& entry) { remapCompressedKeyframe(entry, objectIdMapper); },
                [&objectIdMapper](v1::Creation& entry) { remapObjectSnapshot(entry.object, objectIdMapper); },
                [&objectIdMapper](v1::Deletion& entry) { entry.objectId = objectIdMapper.map(entry.objectId); }},
    data);
}

[[nodiscard]] ObjectNameKey makeObjectNameKey(const RecordingMergeObject& object, const std::string& objectName)
{
  return {object.getSession(), object.getBus(), objectName};
}

[[nodiscard]] RecordingMergeObject makeRecordingMergeObject(const std::filesystem::path& archivePath,
                                                            const v1::ObjectSnapshot& snapshot)
{
  return RecordingMergeObject {
    archivePath, snapshot.objectId, snapshot.session, snapshot.bus, snapshot.name, snapshot.type};
}

bool MergedObjectRegistry::contains(uint32_t objectId) const { return objectById_.count(objectId) != 0U; }

bool MergedObjectRegistry::isIgnored(uint32_t objectId) const { return ignoredObjectIds_.count(objectId) != 0U; }

const std::string* MergedObjectRegistry::findRenamedName(uint32_t objectId) const
{
  const auto name = renamedObjectNames_.find(objectId);
  return name == renamedObjectNames_.end() ? nullptr : &name->second;
}

bool MergedObjectRegistry::hasIgnoredObjects() const noexcept { return !ignoredObjectIds_.empty(); }

bool MergedObjectRegistry::hasRenamedObjects() const noexcept { return !renamedObjectNames_.empty(); }

void MergedObjectRegistry::ignoreObject(uint32_t objectId) { ignoredObjectIds_.insert(objectId); }

void MergedObjectRegistry::renameObject(uint32_t objectId, std::string name)
{
  if (name.empty())
  {
    throwRuntimeError("renamed duplicate object name cannot be empty");
  }

  if (!contains(objectId))
  {
    throwRuntimeError("cannot rename unknown object id " + std::to_string(objectId));
  }

  renamedObjectNames_[objectId] = std::move(name);
}

void MergedObjectRegistry::registerObject(RecordingMergeObject object)
{
  if (contains(object.getObjectId()))
  {
    return;
  }

  auto key = makeObjectNameKey(object, object.getName());
  const auto objectId = object.getObjectId();
  objectById_.emplace(objectId, std::move(object));
  objectIdsByName_[std::move(key)].push_back(objectId);
}

void MergedObjectRegistry::applyDuplicateObjectResolution(const RecordingMergeDuplicateObject& duplicateObject,
                                                          const RecordingMergeDuplicateObjectResolution& resolution)
{
  std::visit(Overloaded {[this, &duplicateObject](const RecordingMergeKeepSelectedObject& keepSelectedObject)
                         {
                           if (keepSelectedObject.selectedObjectIndex >= duplicateObject.objects.size())
                           {
                             throwRuntimeError("selected duplicate object is out of range");
                           }

                           for (std::size_t i = 0U; i < duplicateObject.objects.size(); ++i)
                           {
                             if (i != keepSelectedObject.selectedObjectIndex)
                             {
                               ignoreObject(duplicateObject.objects[i].getObjectId());
                             }
                           }
                         },
                         [this, &duplicateObject](const RecordingMergeRenameObjects& renameObjects)
                         {
                           for (const auto& rename: renameObjects.renamedObjects)
                           {
                             if (rename.objectIndex >= duplicateObject.objects.size())
                             {
                               throwRuntimeError("renamed duplicate object is out of range");
                             }
                             renameObject(duplicateObject.objects[rename.objectIndex].getObjectId(), rename.name);
                           }
                         }},
             resolution);
}

/// Resolves every name collision
void MergedObjectRegistry::resolveDuplicateObjects(const RecordingMergeDuplicateObjectResolver& resolver)
{
  for (const auto& entry: objectIdsByName_)
  {
    const auto& objectIds = entry.second;
    if (objectIds.size() < 2U)
    {
      continue;
    }

    RecordingMergeDuplicateObject duplicateObject;
    duplicateObject.objects.reserve(objectIds.size());
    for (const auto objectId: objectIds)
    {
      duplicateObject.objects.push_back(objectById_.at(objectId));
    }

    if (!resolver)
    {
      const auto& object = duplicateObject.objects.front();
      throwRuntimeError("duplicate object name '" + object.getName() + "' on bus '" + object.getSession() + "." +
                        object.getBus() + "' while merging recordings");
    }

    applyDuplicateObjectResolution(duplicateObject, resolver(duplicateObject));
  }

  validateResolvedObjectNames();
}

void MergedObjectRegistry::validateResolvedObjectNames() const
{
  std::unordered_map<ObjectNameKey, uint32_t, ObjectNameKeyHash> objectIdByName;
  for (const auto& entry: objectById_)
  {
    const auto& object = entry.second;
    const auto objectId = object.getObjectId();
    if (isIgnored(objectId))
    {
      continue;
    }

    const auto* renamedName = findRenamedName(objectId);
    const auto& effectiveName = renamedName == nullptr ? object.getName() : *renamedName;
    const auto [position, inserted] = objectIdByName.emplace(makeObjectNameKey(object, effectiveName), objectId);
    if (!inserted && position->second != objectId)
    {
      throwRuntimeError("duplicate object name '" + effectiveName + "' on bus '" + object.getSession() + "." +
                        object.getBus() + "' after resolving recording merge duplicates");
    }
  }
}

void registerRuntimeObjects(const std::filesystem::path& archivePath,
                            const v1::RuntimeDataEntry& data,
                            MergedObjectRegistry& objectRegistry)
{
  std::visit(Overloaded {[&](const v1::Creation& entry)
                         { objectRegistry.registerObject(makeRecordingMergeObject(archivePath, entry.object)); },
                         [&](const v1::Keyframe& entry)
                         {
                           for (const auto& snapshot: entry.objects)
                           {
                             objectRegistry.registerObject(makeRecordingMergeObject(archivePath, snapshot));
                           }
                         },
                         [&](const v1::CompressedKeyframe& entry)
                         {
                           auto keyframe = readKeyframe(entry);
                           for (const auto& snapshot: keyframe.objects)
                           {
                             objectRegistry.registerObject(makeRecordingMergeObject(archivePath, snapshot));
                           }
                         },
                         [](const auto&) {}},
             data);
}

void updateTimeStats(Summary& summary, const TimeStamp& time)
{
  summary.firstTime = std::min(summary.firstTime, time);
  summary.lastTime = std::max(summary.lastTime, time);
}

[[nodiscard]] bool isKeyframeEntry(const v1::RuntimeDataEntry& data)
{
  return std::holds_alternative<v1::Keyframe>(data) || std::holds_alternative<v1::CompressedKeyframe>(data);
}

[[nodiscard]] std::optional<uint32_t> readObjectId(const v1::RuntimeDataEntry& data)
{
  return std::visit(
    Overloaded {[](const v1::PropertyChange& entry) -> std::optional<uint32_t> { return entry.objectId; },
                [](const v1::Event& entry) -> std::optional<uint32_t> { return entry.objectId; },
                [](const v1::Creation& entry) -> std::optional<uint32_t> { return entry.object.objectId; },
                [](const v1::Deletion& entry) -> std::optional<uint32_t> { return entry.objectId; },
                [](const auto&) -> std::optional<uint32_t> { return std::nullopt; }},
    data);
}

MergedRuntimeEntry::MergedRuntimeEntry(v1::RuntimeDataEntry data, PreparedRuntimeEntry prepared)
  : time_(prepared.time)
  , sortTime_(prepared.time)
  , isKeyframe_(isKeyframeEntry(data))
  , objectId_(prepared.objectId)
  , keyframeObjectIds_(std::move(prepared.keyframeObjectIds))
  , data_(std::move(data))
{
}

void MergedRuntimeEntry::cacheSortKey(const ObjectUseTimeRange& objectUseTimeRange)
{
  sortTime_ = getSortTime(objectUseTimeRange);
  sortPriority_ = getSortPriority();
}

const TimeStamp& MergedRuntimeEntry::getCachedSortTime() const noexcept { return sortTime_; }

uint32_t MergedRuntimeEntry::getCachedSortPriority() const noexcept { return sortPriority_; }

const TimeStamp& MergedRuntimeEntry::getTime() const noexcept { return time_; }

bool MergedRuntimeEntry::isKeyframe() const noexcept { return isKeyframe_; }

const std::optional<uint32_t>& MergedRuntimeEntry::getObjectId() const noexcept { return objectId_; }

const v1::RuntimeDataEntry& MergedRuntimeEntry::getData() const noexcept { return data_; }

[[nodiscard]] std::optional<uint32_t> readCreationObjectId(const v1::RuntimeDataEntry& data)
{
  if (std::holds_alternative<v1::Creation>(data))
  {
    return std::get<v1::Creation>(data).object.objectId;
  }

  return std::nullopt;
}

[[nodiscard]] std::optional<uint32_t> readDeletionObjectId(const v1::RuntimeDataEntry& data)
{
  if (std::holds_alternative<v1::Deletion>(data))
  {
    return std::get<v1::Deletion>(data).objectId;
  }

  return std::nullopt;
}

void updateSummary(Summary& summary, const v1::RuntimeDataEntry& data, const TimeStamp& time)
{
  updateTimeStats(summary, time);

  std::visit(Overloaded {[&summary](const v1::Keyframe&) { ++summary.keyframeCount; },
                         [&summary](const v1::CompressedKeyframe&) { ++summary.keyframeCount; },
                         [&summary](const v1::Creation&) { ++summary.objectCount; },
                         [](const auto&) {}},
             data);
}

[[nodiscard]] std::vector<uint32_t> collectSnapshotObjectIds(const v1::Keyframe& keyframe)
{
  std::vector<uint32_t> objectIds;
  objectIds.reserve(keyframe.objects.size());
  for (const auto& snapshot: keyframe.objects)
  {
    objectIds.push_back(snapshot.objectId);
  }

  return objectIds;
}

void registerSnapshots(const std::filesystem::path& archivePath,
                       const v1::Keyframe& keyframe,
                       MergedObjectRegistry& objectRegistry)
{
  for (const auto& snapshot: keyframe.objects)
  {
    objectRegistry.registerObject(makeRecordingMergeObject(archivePath, snapshot));
  }
}

// A compressed keyframe is unpacked once here and everything the merge needs is taken from that one
// copy: the shift, the id remapping, the object registrations, the entry time and the ids sorting
// will want. Reading each of those separately cost four unpacks and two repacks per keyframe.
[[nodiscard]] PreparedRuntimeEntry prepareRuntimeEntry(v1::RuntimeDataEntry& data,
                                                       const Duration& timeShift,
                                                       ObjectIdMapper& objectIdMapper,
                                                       const std::filesystem::path& archivePath,
                                                       MergedObjectRegistry& objectRegistry)
{
  PreparedRuntimeEntry prepared;

  if (auto* compressed = std::get_if<v1::CompressedKeyframe>(&data))
  {
    auto keyframe = readKeyframe(*compressed);
    if (timeShift != Duration {})
    {
      keyframe.time += timeShift;
    }
    remapKeyframe(keyframe, objectIdMapper);
    registerSnapshots(archivePath, keyframe, objectRegistry);

    prepared.time = keyframe.time;
    prepared.keyframeObjectIds = collectSnapshotObjectIds(keyframe);
    writeCompressedKeyframe(keyframe, *compressed);

    return prepared;
  }

  if (timeShift != Duration {})
  {
    shiftRuntimeData(data, timeShift);
  }
  remapRuntimeData(data, objectIdMapper);
  registerRuntimeObjects(archivePath, data, objectRegistry);

  prepared.time = readEntryTime(data);
  prepared.objectId = readObjectId(data);
  if (const auto* keyframe = std::get_if<v1::Keyframe>(&data))
  {
    prepared.keyframeObjectIds = collectSnapshotObjectIds(*keyframe);
  }

  return prepared;
}

/// Applies the input time shift and ID remapping
std::vector<MergedRuntimeEntry> RecordingArchiveReader::readRuntimeEntries(ObjectIdMapper& objectIdMapper,
                                                                           MergedObjectRegistry& objectRegistry,
                                                                           ProgressTracker& progressTracker) const
{
  const auto& archivePath = inputPlan_.archivePath;
  const auto runtimePath = archivePath / runtimeFileName;
  FilePtr file(openFile(runtimePath));

  const auto fileSize = getArchiveFileSize(file.get(), runtimePath);
  seekArchivePayload(file.get(), runtimePath);

  std::vector<MergedRuntimeEntry> entries;
  while (getCurrentFileOffset(file.get(), runtimePath) < fileSize)
  {
    std::vector<uint8_t> sizeBuffer(SerializationTraits<uint32_t>::serializedSize(0U));
    if (fread(sizeBuffer.data(), 1U, sizeBuffer.size(), file.get()) != sizeBuffer.size())
    {
      throwRuntimeError("could not read runtime entry size from '" + runtimePath.string() + "'");
    }

    uint32_t entrySize = 0U;
    {
      InputStream in(sizeBuffer);
      in.readUInt32(entrySize);
    }

    std::vector<uint8_t> entryBuffer(entrySize);
    if (fread(entryBuffer.data(), 1U, entryBuffer.size(), file.get()) != entryBuffer.size())
    {
      throwRuntimeError("could not read runtime entry from '" + runtimePath.string() + "'");
    }

    v1::RuntimeDataEntry data;
    {
      InputStream in(entryBuffer);
      SerializationTraits<v1::RuntimeDataEntry>::read(in, data);
    }

    auto prepared = prepareRuntimeEntry(data, inputPlan_.timeShift, objectIdMapper, archivePath, objectRegistry);
    entries.emplace_back(std::move(data), std::move(prepared));
    progressTracker.advance(sizeBuffer.size() + entryBuffer.size());
  }

  return entries;
}

std::vector<MergedAnnotationEntry> RecordingArchiveReader::readAnnotationEntries(ProgressTracker& progressTracker) const
{
  const auto& archivePath = inputPlan_.archivePath;
  const auto annotationsPath = archivePath / annotationsFileName;
  FilePtr file(openFile(annotationsPath));

  const auto fileSize = getArchiveFileSize(file.get(), annotationsPath);
  seekArchivePayload(file.get(), annotationsPath);

  const auto applyTimeShift = inputPlan_.timeShift != Duration {};
  std::vector<MergedAnnotationEntry> entries;
  while (getCurrentFileOffset(file.get(), annotationsPath) < fileSize)
  {
    std::vector<uint8_t> sizeBuffer(SerializationTraits<uint32_t>::serializedSize(0U));
    if (fread(sizeBuffer.data(), 1U, sizeBuffer.size(), file.get()) != sizeBuffer.size())
    {
      throwRuntimeError("could not read annotation entry size from '" + annotationsPath.string() + "'");
    }

    uint32_t entrySize = 0U;
    {
      InputStream in(sizeBuffer);
      in.readUInt32(entrySize);
    }

    std::vector<uint8_t> entryBuffer(entrySize);
    if (fread(entryBuffer.data(), 1U, entryBuffer.size(), file.get()) != entryBuffer.size())
    {
      throwRuntimeError("could not read annotation entry from '" + annotationsPath.string() + "'");
    }

    v1::Annotation annotation;
    {
      InputStream in(entryBuffer);
      SerializationTraits<v1::Annotation>::read(in, annotation);
    }

    if (applyTimeShift)
    {
      annotation.time += inputPlan_.timeShift;
    }

    MergedAnnotationEntry entry;
    entry.time = annotation.time;
    if (applyTimeShift)
    {
      entry.bytes = writeSizedEntryToBytes(annotation);
    }
    else
    {
      entry.bytes.reserve(sizeBuffer.size() + entryBuffer.size());
      entry.bytes.insert(entry.bytes.end(), sizeBuffer.begin(), sizeBuffer.end());
      entry.bytes.insert(entry.bytes.end(), entryBuffer.begin(), entryBuffer.end());
    }
    entries.push_back(std::move(entry));
    progressTracker.advance(sizeBuffer.size() + entryBuffer.size());
  }

  return entries;
}

void RecordingArchiveReader::readIndexDefinitions(IndexedObjectRegistry& indexedObjects,
                                                  ObjectIdMapper& objectIdMapper) const
{
  const auto& archivePath = inputPlan_.archivePath;
  auto indexesBuffer = readFileTail(archivePath / indexesFileName);
  InputStream in(indexesBuffer);

  while (!in.atEnd())
  {
    v1::IndexFileEntry entry;
    SerializationTraits<v1::IndexFileEntry>::read(in, entry);

    if (std::holds_alternative<v1::IndexDefinition>(entry))
    {
      auto definition = std::get<v1::IndexDefinition>(std::move(entry));
      definition.objectId = objectIdMapper.map(definition.objectId);
      definition.indexId = definition.objectId;
      indexedObjects.addDefinition(std::move(definition), archivePath);
    }
  }
}

void appendBytes(FILE* file, const std::vector<uint8_t>& bytes)
{
  if (!bytes.empty())
  {
    doWrite(bytes, file);
  }
}

[[nodiscard]] Summary makeInitialSummary()
{
  Summary summary;
  summary.firstTime = TimeStamp(Duration(std::numeric_limits<Duration::ValueType>::max()));
  summary.lastTime = TimeStamp(Duration(std::numeric_limits<Duration::ValueType>::min()));
  summary.keyframeCount = 0U;
  summary.objectCount = 0U;
  summary.typeCount = 0U;
  summary.annotationCount = 0U;
  summary.indexedObjectCount = 0U;
  return summary;
}

/// Owns the merged recording and applies all final transformations.
class MergedRecording
{
public:
  void reserveInputCount(std::size_t inputCount);
  void finalize(const RecordingMergeDuplicateObjectResolver& duplicateObjectResolver);

  [[nodiscard]] const Summary& getSummary() const noexcept;
  [[nodiscard]] const IndexedObjectRegistry& getIndexedObjects() const noexcept;
  [[nodiscard]] const std::vector<MergedRuntimeEntry>& getRuntimeEntries() const noexcept;
  [[nodiscard]] const std::vector<MergedAnnotationEntry>& getAnnotations() const noexcept;
  [[nodiscard]] const std::vector<std::vector<uint8_t>>& getTypeFileTails() const noexcept;

private:
  friend class RecordingArchiveReader;

  void removeIgnoredRuntimeEntries();
  void applyObjectRenames();
  void sortRuntimeEntries();
  void rebuildSummary();

private:
  Summary summary_ = makeInitialSummary();
  IndexedObjectRegistry indexedObjects_;
  MergedObjectRegistry objectRegistry_;
  std::vector<MergedRuntimeEntry> runtimeEntries_;
  std::vector<MergedAnnotationEntry> annotations_;
  std::vector<std::vector<uint8_t>> typeFileTails_;
  uint32_t nextObjectId_ = 1U;
};

[[nodiscard]] bool isEmptySummary(const Summary& summary) { return summary.lastTime < summary.firstTime; }

[[nodiscard]] Duration calculateTimeShift(const RecordingMergeInput& inputArchive, RecordingMergeMode mode)
{
  if (mode == RecordingMergeMode::normalMerge)
  {
    return {};
  }

  const auto summary = readArchiveSummary(inputArchive.archivePath);
  if (isEmptySummary(summary))
  {
    return {};
  }

  if (mode == RecordingMergeMode::zeroAligned)
  {
    return TimeStamp(Duration {}) - summary.firstTime;
  }

  if (mode == RecordingMergeMode::offsetAligned)
  {
    return TimeStamp(inputArchive.offset) - summary.firstTime;
  }

  throwRuntimeError("unknown recording merge mode");
  return {};
}

[[nodiscard]] std::vector<InputArchivePlan> makeInputPlans(const RecordingMergeSettings& settings)
{
  if (settings.inputArchives.empty())
  {
    throwRuntimeError("at least one input recording archive is required");
  }

  std::vector<InputArchivePlan> inputPlans;
  inputPlans.reserve(settings.inputArchives.size());
  for (const auto& inputArchive: settings.inputArchives)
  {
    if (!std::filesystem::is_directory(inputArchive.archivePath))
    {
      throwRuntimeError("'" + inputArchive.archivePath.string() + "' is not a valid recording archive directory");
    }

    InputArchivePlan inputPlan;
    inputPlan.archivePath = inputArchive.archivePath;
    inputPlan.timeShift = calculateTimeShift(inputArchive, settings.mode);
    inputPlans.push_back(std::move(inputPlan));
  }

  return inputPlans;
}

void RecordingArchiveReader::readInto(MergedRecording& mergedRecording, ProgressTracker& progressTracker) const
{
  // Each archive needs an independent source-ID map
  ObjectIdMapper objectIdMapper(mergedRecording.nextObjectId_);
  readIndexDefinitions(mergedRecording.indexedObjects_, objectIdMapper);

  auto entries = readRuntimeEntries(objectIdMapper, mergedRecording.objectRegistry_, progressTracker);
  mergedRecording.runtimeEntries_.insert(mergedRecording.runtimeEntries_.end(),
                                         std::make_move_iterator(entries.begin()),
                                         std::make_move_iterator(entries.end()));

  auto annotations = readAnnotationEntries(progressTracker);
  mergedRecording.annotations_.insert(mergedRecording.annotations_.end(),
                                      std::make_move_iterator(annotations.begin()),
                                      std::make_move_iterator(annotations.end()));

  auto typeFileTail = readFileTail(inputPlan_.archivePath / typesFileName);
  mergedRecording.summary_.typeCount += countTypeSpecs(typeFileTail, inputPlan_.archivePath);
  mergedRecording.typeFileTails_.push_back(std::move(typeFileTail));
}

/// Returns false only when filtering empties a keyframe that originally contained objects
[[nodiscard]] bool removeIgnoredObjectSnapshots(v1::Keyframe& keyframe, const MergedObjectRegistry& objectRegistry)
{
  const auto originalObjectCount = keyframe.objects.size();
  keyframe.objects.erase(
    std::remove_if(keyframe.objects.begin(),
                   keyframe.objects.end(),
                   [&objectRegistry](const auto& snapshot) { return objectRegistry.isIgnored(snapshot.objectId); }),
    keyframe.objects.end());
  return !keyframe.objects.empty() || originalObjectCount == 0U;
}

[[nodiscard]] bool removeIgnoredObjectSnapshots(v1::CompressedKeyframe& entry,
                                                const MergedObjectRegistry& objectRegistry)
{
  auto keyframe = readKeyframe(entry);
  const auto keepKeyframe = removeIgnoredObjectSnapshots(keyframe, objectRegistry);
  writeCompressedKeyframe(keyframe, entry);
  return keepKeyframe;
}

/// Filters keyframe snapshots and rejects entries that reference ignored or unregistered objects
bool MergedRuntimeEntry::removeIgnoredObjects(const MergedObjectRegistry& objectRegistry)
{
  return std::visit(
    Overloaded {
      [&objectRegistry](const v1::PropertyChange& entry)
      { return !objectRegistry.isIgnored(entry.objectId) && objectRegistry.contains(entry.objectId); },
      [&objectRegistry](const v1::Event& entry)
      { return !objectRegistry.isIgnored(entry.objectId) && objectRegistry.contains(entry.objectId); },
      [&objectRegistry](v1::Keyframe& entry) { return removeIgnoredObjectSnapshots(entry, objectRegistry); },
      [&objectRegistry](v1::CompressedKeyframe& entry) { return removeIgnoredObjectSnapshots(entry, objectRegistry); },
      [&objectRegistry](const v1::Creation& entry)
      { return !objectRegistry.isIgnored(entry.object.objectId) && objectRegistry.contains(entry.object.objectId); },
      [&objectRegistry](const v1::Deletion& entry)
      { return !objectRegistry.isIgnored(entry.objectId) && objectRegistry.contains(entry.objectId); }},
    data_);
}

void MergedRecording::removeIgnoredRuntimeEntries()
{
  std::vector<MergedRuntimeEntry> keptEntries;
  keptEntries.reserve(runtimeEntries_.size());
  for (auto& entry: runtimeEntries_)
  {
    if (!entry.removeIgnoredObjects(objectRegistry_))
    {
      continue;
    }

    keptEntries.push_back(std::move(entry));
  }

  runtimeEntries_ = std::move(keptEntries);
}

void IndexedObjectRegistry::removeIgnoredDefinitions(const MergedObjectRegistry& objectRegistry)
{
  if (!objectRegistry.hasIgnoredObjects())
  {
    return;
  }

  IndexedObjectRegistry indexedObjects;
  for (auto& definition: definitions_)
  {
    if (objectRegistry.isIgnored(definition.objectId))
    {
      continue;
    }

    indexedObjects.addDefinition(std::move(definition), {});
  }

  *this = std::move(indexedObjects);
}

void renameObjectSnapshot(v1::ObjectSnapshot& snapshot, const MergedObjectRegistry& objectRegistry)
{
  if (const auto* name = objectRegistry.findRenamedName(snapshot.objectId))
  {
    snapshot.name = *name;
  }
}

void renameObjectSnapshots(v1::Keyframe& keyframe, const MergedObjectRegistry& objectRegistry)
{
  for (auto& snapshot: keyframe.objects)
  {
    renameObjectSnapshot(snapshot, objectRegistry);
  }
}

void renameObjectSnapshots(v1::CompressedKeyframe& entry, const MergedObjectRegistry& objectRegistry)
{
  auto keyframe = readKeyframe(entry);
  renameObjectSnapshots(keyframe, objectRegistry);
  writeCompressedKeyframe(keyframe, entry);
}

void MergedRuntimeEntry::applyObjectRenames(const MergedObjectRegistry& objectRegistry)
{
  std::visit(
    Overloaded {[&objectRegistry](v1::Creation& entry) { renameObjectSnapshot(entry.object, objectRegistry); },
                [&objectRegistry](v1::Keyframe& entry) { renameObjectSnapshots(entry, objectRegistry); },
                [&objectRegistry](v1::CompressedKeyframe& entry) { renameObjectSnapshots(entry, objectRegistry); },
                [](auto&) {}},
    data_);
}

void IndexedObjectRegistry::applyObjectRenames(const MergedObjectRegistry& objectRegistry)
{
  for (auto& definition: definitions_)
  {
    if (const auto* name = objectRegistry.findRenamedName(definition.objectId))
    {
      definition.objectName = *name;
    }
  }
}

void MergedRecording::applyObjectRenames()
{
  if (!objectRegistry_.hasRenamedObjects())
  {
    return;
  }

  indexedObjects_.applyObjectRenames(objectRegistry_);
  for (auto& entry: runtimeEntries_)
  {
    entry.applyObjectRenames(objectRegistry_);
  }
}

/// Tracks the use of objects so entries can be sorted on their first and last dependent entry
class ObjectUseTimeRange
{
public:
  [[nodiscard]] static ObjectUseTimeRange collect(const std::vector<MergedRuntimeEntry>& entries);

  void update(uint32_t objectId, const TimeStamp& time);
  [[nodiscard]] const TimeStamp* findFirstUseTime(uint32_t objectId) const;
  [[nodiscard]] const TimeStamp* findLastUseTime(uint32_t objectId) const;

private:
  std::unordered_map<uint32_t, TimeStamp> firstUseTimeByObjectId_;
  std::unordered_map<uint32_t, TimeStamp> lastUseTimeByObjectId_;
};

void ObjectUseTimeRange::update(uint32_t objectId, const TimeStamp& time)
{
  const auto firstUseTime = firstUseTimeByObjectId_.find(objectId);
  if (firstUseTime == firstUseTimeByObjectId_.end())
  {
    firstUseTimeByObjectId_.emplace(objectId, time);
  }
  else if (time < firstUseTime->second)
  {
    firstUseTime->second = time;
  }

  const auto lastUseTime = lastUseTimeByObjectId_.find(objectId);
  if (lastUseTime == lastUseTimeByObjectId_.end())
  {
    lastUseTimeByObjectId_.emplace(objectId, time);
  }
  else if (lastUseTime->second < time)
  {
    lastUseTime->second = time;
  }
}

const TimeStamp* ObjectUseTimeRange::findFirstUseTime(uint32_t objectId) const
{
  const auto time = firstUseTimeByObjectId_.find(objectId);
  return time == firstUseTimeByObjectId_.end() ? nullptr : &time->second;
}

const TimeStamp* ObjectUseTimeRange::findLastUseTime(uint32_t objectId) const
{
  const auto time = lastUseTimeByObjectId_.find(objectId);
  return time == lastUseTimeByObjectId_.end() ? nullptr : &time->second;
}

void MergedRuntimeEntry::updateObjectUseTimeRange(ObjectUseTimeRange& objectUseTimeRange) const
{
  for (const auto objectId: keyframeObjectIds_)
  {
    objectUseTimeRange.update(objectId, time_);
  }

  std::visit(Overloaded {[&](const v1::PropertyChange& data) { objectUseTimeRange.update(data.objectId, time_); },
                         [&](const v1::Event& data) { objectUseTimeRange.update(data.objectId, time_); },
                         [&](const v1::Deletion& data) { objectUseTimeRange.update(data.objectId, time_); },
                         [](const auto&) {}},
             data_);
}

ObjectUseTimeRange ObjectUseTimeRange::collect(const std::vector<MergedRuntimeEntry>& entries)
{
  ObjectUseTimeRange objectUseTimeRange;
  for (const auto& entry: entries)
  {
    entry.updateObjectUseTimeRange(objectUseTimeRange);
  }

  return objectUseTimeRange;
}

TimeStamp MergedRuntimeEntry::getSortTime(const ObjectUseTimeRange& objectUseTimeRange) const
{
  if (const auto objectId = readCreationObjectId(data_))
  {
    if (const auto* firstUseTime = objectUseTimeRange.findFirstUseTime(objectId.value());
        firstUseTime != nullptr && *firstUseTime < time_)
    {
      return *firstUseTime;
    }
  }

  if (const auto objectId = readDeletionObjectId(data_))
  {
    if (const auto* lastUseTime = objectUseTimeRange.findLastUseTime(objectId.value());
        lastUseTime != nullptr && time_ < *lastUseTime)
    {
      return *lastUseTime;
    }
  }

  return time_;
}

uint32_t MergedRuntimeEntry::getSortPriority() const
{
  if (readCreationObjectId(data_).has_value())
  {
    return 0U;
  }

  if (readDeletionObjectId(data_).has_value())
  {
    return 2U;
  }

  return 1U;
}

/// Orders creations before their first use
void MergedRecording::sortRuntimeEntries()
{
  const auto objectUseTimeRange = ObjectUseTimeRange::collect(runtimeEntries_);
  for (auto& entry: runtimeEntries_)
  {
    entry.cacheSortKey(objectUseTimeRange);
  }

  std::stable_sort(runtimeEntries_.begin(),
                   runtimeEntries_.end(),
                   [](const auto& lhs, const auto& rhs)
                   {
                     if (lhs.getCachedSortTime() != rhs.getCachedSortTime())
                     {
                       return lhs.getCachedSortTime() < rhs.getCachedSortTime();
                     }

                     return lhs.getCachedSortPriority() < rhs.getCachedSortPriority();
                   });
}

void MergedRecording::rebuildSummary()
{
  const auto typeCount = summary_.typeCount;

  summary_ = makeInitialSummary();
  summary_.typeCount = typeCount;
  for (const auto& entry: runtimeEntries_)
  {
    updateSummary(summary_, entry.getData(), entry.getTime());
  }
  for (const auto& annotation: annotations_)
  {
    updateTimeStats(summary_, annotation.time);
    ++summary_.annotationCount;
  }
  summary_.indexedObjectCount = std_util::checkedConversion<uint32_t>(indexedObjects_.getDefinitions().size());
}

void MergedRecording::reserveInputCount(std::size_t inputCount) { typeFileTails_.reserve(inputCount); }

/// Applies duplicate resolution in dependency order
void MergedRecording::finalize(const RecordingMergeDuplicateObjectResolver& duplicateObjectResolver)
{
  // Later phases rely on the decisions made by earlier phases; keep this transformation order centralized here.
  objectRegistry_.resolveDuplicateObjects(duplicateObjectResolver);
  indexedObjects_.removeIgnoredDefinitions(objectRegistry_);
  removeIgnoredRuntimeEntries();
  applyObjectRenames();
  sortRuntimeEntries();

  std::stable_sort(
    annotations_.begin(), annotations_.end(), [](const auto& lhs, const auto& rhs) { return lhs.time < rhs.time; });

  rebuildSummary();
}

const Summary& MergedRecording::getSummary() const noexcept { return summary_; }

const IndexedObjectRegistry& MergedRecording::getIndexedObjects() const noexcept { return indexedObjects_; }

const std::vector<MergedRuntimeEntry>& MergedRecording::getRuntimeEntries() const noexcept { return runtimeEntries_; }

const std::vector<MergedAnnotationEntry>& MergedRecording::getAnnotations() const noexcept { return annotations_; }

const std::vector<std::vector<uint8_t>>& MergedRecording::getTypeFileTails() const noexcept { return typeFileTails_; }

std::optional<uint32_t> IndexedObjectRegistry::findIndexId(uint32_t objectId) const
{
  const auto indexId = indexIdByObjectId_.find(objectId);
  if (indexId == indexIdByObjectId_.end())
  {
    return std::nullopt;
  }

  return indexId->second;
}

const std::vector<v1::IndexDefinition>& IndexedObjectRegistry::getDefinitions() const noexcept { return definitions_; }

/// Initializes an archive file with its header
class ArchiveOutputFile
{
public:
  ArchiveOutputFile(std::filesystem::path path, v1::FileKind fileKind): path_(std::move(path)), file_(createFile(path_))
  {
    writeArchiveHeader(file_.get(), fileKind);
  }

  template <typename T>
  void writeValue(T value)
  {
    write<T>(std::move(value), file_.get());
  }

  void append(const std::vector<uint8_t>& bytes) { appendBytes(file_.get(), bytes); }

  [[nodiscard]] std::size_t getOffset() const { return getCurrentFileOffset(file_.get(), path_); }

private:
  std::filesystem::path path_;
  FilePtr file_;
};

/// Serializes a merged recording into the files that form an archive
class RecordingArchiveWriter
{
public:
  explicit RecordingArchiveWriter(std::filesystem::path outputArchive): outputArchive_(std::move(outputArchive)) {}

  void write(const MergedRecording& mergedRecording) const;

private:
  void writeRuntimeAndIndexes(const MergedRecording& mergedRecording) const;
  void writeTypes(const std::vector<std::vector<uint8_t>>& typeFileTails) const;
  void writeAnnotations(const std::vector<MergedAnnotationEntry>& annotations) const;
  void writeSummary(const Summary& summary) const;

private:
  std::filesystem::path outputArchive_;
};

/// Writes runtime entries
void RecordingArchiveWriter::writeRuntimeAndIndexes(const MergedRecording& mergedRecording) const
{
  ArchiveOutputFile indexesFile(outputArchive_ / indexesFileName, v1::FileKind::indexesFile);
  for (const auto& definition: mergedRecording.getIndexedObjects().getDefinitions())
  {
    indexesFile.writeValue(v1::IndexFileEntry {definition});
  }

  ArchiveOutputFile runtimeFile(outputArchive_ / runtimeFileName, v1::FileKind::dataFile);
  for (const auto& entry: mergedRecording.getRuntimeEntries())
  {
    const auto runtimeOffset = static_cast<uint64_t>(runtimeFile.getOffset());
    runtimeFile.append(writeSizedEntryToBytes(entry.getData()));

    if (entry.isKeyframe())
    {
      indexesFile.writeValue(v1::IndexFileEntry {v1::Index {entry.getTime(), keyframeIndexId, runtimeOffset}});
    }

    if (const auto& objectId = entry.getObjectId(); objectId.has_value())
    {
      if (const auto indexId = mergedRecording.getIndexedObjects().findIndexId(objectId.value()))
      {
        indexesFile.writeValue(v1::IndexFileEntry {v1::Index {entry.getTime(), indexId.value(), runtimeOffset}});
      }
    }
  }
}

void RecordingArchiveWriter::writeTypes(const std::vector<std::vector<uint8_t>>& typeFileTails) const
{
  ArchiveOutputFile typesFile(outputArchive_ / typesFileName, v1::FileKind::typesFile);
  for (const auto& tail: typeFileTails)
  {
    typesFile.append(tail);
  }
}

void RecordingArchiveWriter::writeAnnotations(const std::vector<MergedAnnotationEntry>& annotations) const
{
  ArchiveOutputFile annotationsFile(outputArchive_ / annotationsFileName, v1::FileKind::annotationsFile);
  for (const auto& annotation: annotations)
  {
    annotationsFile.append(annotation.bytes);
  }
}

void RecordingArchiveWriter::writeSummary(const Summary& summary) const
{
  ArchiveOutputFile summaryFile(outputArchive_ / summaryFileName, v1::FileKind::summaryFile);
  summaryFile.writeValue(summary);
}

void RecordingArchiveWriter::write(const MergedRecording& mergedRecording) const
{
  writeRuntimeAndIndexes(mergedRecording);
  writeTypes(mergedRecording.getTypeFileTails());
  writeAnnotations(mergedRecording.getAnnotations());
  writeSummary(mergedRecording.getSummary());
}

[[nodiscard]] bool holdsRecordingArchive(const std::filesystem::path& archivePath)
{
  return std::filesystem::exists(archivePath / indexesFileName) ||
         std::filesystem::exists(archivePath / runtimeFileName) ||
         std::filesystem::exists(archivePath / typesFileName) ||
         std::filesystem::exists(archivePath / annotationsFileName) ||
         std::filesystem::exists(archivePath / summaryFileName);
}

// Both checks run before anything is written. The inputs are all read before the first byte goes
// out, so an output that is also an input does not corrupt a read -- it destroys the source if the
// write then fails, and there is nothing to fall back on.
void validateOutputArchive(const RecordingMergeSettings& settings)
{
  std::error_code errorCode;
  const auto outputPath = std::filesystem::weakly_canonical(settings.outputArchive, errorCode);
  if (errorCode)
  {
    throwRuntimeError("could not resolve the output recording archive path '" + settings.outputArchive.string() +
                      "'. " + errorCode.message());
  }

  for (const auto& inputArchive: settings.inputArchives)
  {
    std::error_code inputErrorCode;
    const auto inputPath = std::filesystem::weakly_canonical(inputArchive.archivePath, inputErrorCode);
    if (!inputErrorCode && inputPath == outputPath)
    {
      throwRuntimeError("the output recording archive '" + settings.outputArchive.string() +
                        "' is also an input; choose a different output");
    }
  }

  if (!settings.force && holdsRecordingArchive(settings.outputArchive))
  {
    throwRuntimeError("'" + settings.outputArchive.string() +
                      "' already holds a recording archive; pass --force to replace it");
  }
}

void createOutputDirectory(const std::filesystem::path& outputArchive)
{
  std::error_code errorCode;
  std::filesystem::create_directories(outputArchive, errorCode);
  if (errorCode)
  {
    throwRuntimeError("could not create output recording archive directory '" + outputArchive.string() + "'. " +
                      errorCode.message());
  }

  if (!std::filesystem::is_directory(outputArchive))
  {
    throwRuntimeError("'" + outputArchive.string() + "' is not a valid output recording archive directory");
  }
}

}  // namespace

void mergeRecordings(const RecordingMergeSettings& settings)
{
  const auto inputPlans = makeInputPlans(settings);
  validateOutputArchive(settings);
  createOutputDirectory(settings.outputArchive);

  std::size_t totalInputBytes = 0U;
  for (const auto& inputPlan: inputPlans)
  {
    totalInputBytes += fileSizeOrZero(inputPlan.archivePath / runtimeFileName);
    totalInputBytes += fileSizeOrZero(inputPlan.archivePath / annotationsFileName);
  }
  ProgressTracker progressTracker(settings.progressReporter, totalInputBytes);

  MergedRecording mergedRecording;
  mergedRecording.reserveInputCount(inputPlans.size());
  for (const auto& inputPlan: inputPlans)
  {
    RecordingArchiveReader(inputPlan).readInto(mergedRecording, progressTracker);
  }

  mergedRecording.finalize(settings.duplicateObjectResolver);
  RecordingArchiveWriter(settings.outputArchive).write(mergedRecording);
  progressTracker.finish();
}

}  // namespace sen::db
