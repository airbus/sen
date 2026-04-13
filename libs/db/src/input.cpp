// === input.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/input.h"

// implementation
#include "constants.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/object.h"
#include "sen/db/annotation.h"
#include "sen/db/creation.h"
#include "sen/db/deletion.h"
#include "sen/db/event.h"
#include "sen/db/keyframe.h"
#include "sen/db/property_change.h"
#include "sen/db/snapshot.h"
#include "sen/kernel/type_specs_utils.h"

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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS  // NOLINT
#  pragma warning(disable: 4996)   // NOLINT
#endif

namespace sen::db
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

void checkDirectory(const std::filesystem::path& path)
{
  if (!std::filesystem::is_directory(path))
  {
    std::string err;
    err.append("'");
    err.append(path.string());
    err.append("' is not a valid directory");
    throwRuntimeError(err);
  }
}

//--------------------------------------------------------------------------------------------------------------
// ObjectIndex
//--------------------------------------------------------------------------------------------------------------

struct ObjectIndex
{
  uint64_t offset = 0U;
};

using ObjectIndexList = std::vector<ObjectIndex>;

//--------------------------------------------------------------------------------------------------------------
// IndexedCursorState
//--------------------------------------------------------------------------------------------------------------

class DataCursorState
{
  SEN_MOVE_ONLY(DataCursorState)

public:
  explicit DataCursorState(const std::filesystem::path& path): file_(openFile(path))
  {
    // get to the first entry
    init(getFileHeaderSize());
  }

  DataCursorState(const std::filesystem::path& path, uint64_t offset): file_(openFile(path))
  {
    // get to the specified offset
    init(offset);
  }

  ~DataCursorState()
  {
    fclose(file_);  // NOLINT
  }

public:
  [[nodiscard]] FILE* getFile() const noexcept { return file_; }

  [[nodiscard]] std::size_t getFileSize() const noexcept { return fileSize_; }

private:
  void init(uint64_t offset)
  {
    // get the size
    fseek(file_, 0, SEEK_END);
    fileSize_ = ftell(file_);

    // move to entry
    fseek(file_, static_cast<uint32_t>(offset), SEEK_SET);
  }

private:
  FILE* file_;
  std::size_t fileSize_ = 0U;
};

//--------------------------------------------------------------------------------------------------------------
// IndexedCursorState
//--------------------------------------------------------------------------------------------------------------

class IndexedCursorState
{
  SEN_NOCOPY_NOMOVE(IndexedCursorState)

public:
  explicit IndexedCursorState(const std::filesystem::path& path, const ObjectIndexList& indexes)
    : file_(openFile(path)), indexes_(indexes), nextPosition_(indexes_.begin())
  {
  }

  ~IndexedCursorState()
  {
    fclose(file_);  // NOLINT
  }

public:
  [[nodiscard]] FILE* getFile() const noexcept { return file_; }

  [[nodiscard]] bool advance()
  {
    if (nextPosition_ == indexes_.end())
    {
      return false;
    }

    fseek(file_, static_cast<uint32_t>(nextPosition_->offset), SEEK_SET);

    ++nextPosition_;
    return true;
  }

private:
  FILE* file_;
  const ObjectIndexList& indexes_;
  ObjectIndexList::const_iterator nextPosition_;
};

//--------------------------------------------------------------------------------------------------------------
// AnnotationCursorState
//--------------------------------------------------------------------------------------------------------------

class AnnotationCursorState
{
  SEN_MOVE_ONLY(AnnotationCursorState)

public:
  explicit AnnotationCursorState(const std::filesystem::path& path): file_(openFile(path))
  {
    // get to the first entry
    init(getFileHeaderSize());
  }

  ~AnnotationCursorState()
  {
    fclose(file_);  // NOLINT
  }

public:
  [[nodiscard]] FILE* getFile() const noexcept { return file_; }

  [[nodiscard]] std::size_t getFileSize() const noexcept { return fileSize_; }

private:
  void init(uint64_t offset)
  {
    // get the size
    fseek(file_, 0, SEEK_END);
    fileSize_ = ftell(file_);

    // move to entry
    fseek(file_, static_cast<uint32_t>(offset), SEEK_SET);
  }

private:
  FILE* file_;
  std::size_t fileSize_ = 0U;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Input::Impl
//--------------------------------------------------------------------------------------------------------------

class Input::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  explicit Impl(std::filesystem::path path, CustomTypeRegistry& nativeTypes)
    : path_(std::move(path)), kernelTypes_(nativeTypes)
  {
    // basic checks
    checkDirectory(path_);
    checkRuntimeData();
    checkAnnotations();

    // prefetch metadata
    readTypes();
    readSummary();
  }

  ~Impl() = default;

public:
  [[nodiscard]] const std::filesystem::path& getPath() const noexcept { return path_; }

  [[nodiscard]] const Summary& getSummary() const noexcept { return summary_; }

  [[nodiscard]] const CustomTypeRegistry& getTypes() const noexcept { return kernelTypes_; }

  [[nodiscard]] AnnotationCursor annotationsBegin() const
  {
    auto cursorState = std::make_shared<AnnotationCursorState>(path_ / annotationsFileName);

    return AnnotationCursor([state = std::move(cursorState), this](auto& entry)
                            { entry = provideAnnotationFront(*state); });
  }

  [[nodiscard]] DataCursor begin()
  {
    auto state = std::make_shared<DataCursorState>(path_ / runtimeFileName);
    return DataCursor([theState = std::move(state), this](auto& entry) { entry = provideRuntimeDataFront(*theState); });
  }

  [[nodiscard]] DataCursor at(const KeyframeIndex& index)
  {
    auto state = std::make_shared<DataCursorState>(path_ / runtimeFileName, index.offset);
    return DataCursor([theState = std::move(state), this](auto& entry) { entry = provideRuntimeDataFront(*theState); });
  }

  [[nodiscard]] DataCursor makeCursor(const ObjectIndexDef& indexDef)
  {
    auto indexedState = std::make_shared<IndexedCursorState>(path_ / runtimeFileName, objectIndexes_[indexDef.indexId]);

    return DataCursor(
      [state = std::move(indexedState), this](auto& entry)
      {
        if (state->advance())
        {
          entry = readRuntimeDataEntry(state->getFile());
        }
        else
        {
          entry.time = {};
          entry.payload = End();
        }
      });
  }

  [[nodiscard]] Span<const ObjectIndexDef> getObjectIndexDefinitions()
  {
    readIndexes();
    return objectIndexDefinitions_;
  }

  [[nodiscard]] Span<const KeyframeIndex> getAllKeyframeIndexes()
  {
    readIndexes();
    return keyframeIndexes_;
  }

  [[nodiscard]] std::optional<KeyframeIndex> getClosestKeyframeIndex(TimeStamp time)
  {
    readIndexes();

    auto iterGeq = std::lower_bound(keyframeIndexes_.begin(),
                                    keyframeIndexes_.end(),
                                    time,
                                    [](const KeyframeIndex& lhs, const TimeStamp& rhs) { return lhs.time < rhs; });

    if (iterGeq == keyframeIndexes_.begin())
    {
      return *iterGeq;
    }

    const auto& prev = *(iterGeq - 1);
    const auto a = prev.time.sinceEpoch().get();
    const auto b = iterGeq->time.sinceEpoch().get();
    const auto t = time.sinceEpoch().get();

    if (llabs(t - a) < llabs(t - b))
    {
      return prev;
    }

    return *iterGeq;
  }

  void addAnnotation(TimeStamp time, const Type* type, std::vector<uint8_t>&& value) const
  {
    const auto filePath = path_ / annotationsFileName;

    // binary append mode
    auto* file = fopen(filePath.string().c_str(), "ab");  // NOLINT
    if (file == nullptr)
    {
      const auto pathStr = filePath.string();
      auto* description = strerror(errno);
      getLogger()->error("could not open file {}. {}", pathStr, description);

      std::string err;
      err.append("could not open file '");
      err.append(pathStr);
      err.append("'. ");
      err.append(description);
      throwRuntimeError(err);
    }

    v1::Annotation data {};
    data.time = time;
    data.type = type->isCustomType() ? type->asCustomType()->getQualifiedName() : type->getName();
    data.value = std::move(value);

    auto buf = writeSizeAndDataToBuffer(data);
    doWrite(buf->getConstSpan(), file);
    fclose(file);  // NOLINT(cppcoreguidelines-owning-memory)
  }

private:
  void checkRuntimeData() const
  {
    const auto filePath = path_ / runtimeFileName;

    std::vector<uint8_t> buffer;
    readPartialFile(buffer, getFileHeaderSize(), filePath);

    InputStream in(buffer);
    tryReadMagicAndHeader(in, filePath, beginMagic, v1::FileKind::dataFile);
  }

  void checkAnnotations() const
  {
    const auto filePath = path_ / annotationsFileName;

    std::vector<uint8_t> buffer;
    readPartialFile(buffer, getFileHeaderSize(), filePath);

    InputStream in(buffer);
    tryReadMagicAndHeader(in, filePath, beginMagic, v1::FileKind::annotationsFile);
  }

  void readSummary()
  {
    const auto filePath = path_ / summaryFileName;
    std::vector<uint8_t> buffer;
    readFullFile(buffer, filePath);

    InputStream in(buffer);
    tryReadMagicAndHeader(in, filePath, beginMagic, v1::FileKind::summaryFile);
    tryReadSummary(in);
  }

  void readTypes()
  {
    const auto filePath = path_ / typesFileName;
    std::vector<uint8_t> buffer;
    readFullFile(buffer, filePath);

    InputStream in(buffer);
    tryReadMagicAndHeader(in, filePath, beginMagic, v1::FileKind::typesFile);
    tryReadTypes(in);
  }

  void readIndexes()
  {
    if (indexesRead_)
    {
      return;
    }

    const auto filePath = path_ / indexesFileName;
    std::vector<uint8_t> buffer;
    readFullFile(buffer, filePath);

    InputStream in(buffer);
    tryReadMagicAndHeader(in, filePath, beginMagic, v1::FileKind::indexesFile);

    // reserve according to summary
    keyframeIndexes_.reserve(summary_.keyframeCount);
    objectIndexDefinitions_.reserve(summary_.indexedObjectCount);

    // read the content
    tryReadIndexes(in);

    // be totally sure that keyframes are sorted by time
    std::sort(keyframeIndexes_.begin(),
              keyframeIndexes_.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.time < rhs.time; });

    indexesRead_ = true;
  }

  static void readFullFile(std::vector<uint8_t>& buffer, const std::filesystem::path& path)
  {
    auto* file = openFile(path);

    // get the size
    fseek(file, 0U, SEEK_END);
    const auto fileSize = ftell(file);
    fseek(file, 0U, SEEK_SET);

    // allocate buffer
    buffer.resize(fileSize);

    // read
    if (auto readed = fread(buffer.data(), 1U, buffer.size(), file); readed != buffer.size())
    {
      throwRuntimeError(strerror(errno));
    }

    std::ignore = fclose(file);  // NOLINT
  }

  static void readPartialFile(std::vector<uint8_t>& buffer, std::size_t size, const std::filesystem::path& path)
  {
    auto* file = openFile(path);

    // allocate buffer
    buffer.resize(size);

    // read
    if (auto readed = fread(buffer.data(), 1U, buffer.size(), file); readed != buffer.size())
    {
      throwRuntimeError(strerror(errno));
    }

    std::ignore = fclose(file);  // NOLINT
  }

  /// Reads a v1::Magic and a v1::FileHeader from a recording file.
  /// Returns the KernelProtocolVersion of the file (stored in the v1::FileHeader::version field)
  static void readMagicAndHeaderV1(InputStream& in,
                                   const std::filesystem::path& filePath,
                                   uint32_t expectedMagic,
                                   v1::FileKind expectedKind)
  {
    // magic
    {
      v1::Magic magic;
      SerializationTraits<decltype(magic)>::read(in, magic);

      if (magic.value != expectedMagic)
      {
        std::string err;
        err.append("invalid magic number ");
        err.append(std::to_string(magic.value));
        err.append(" for file '");
        err.append(filePath.string());
        err.append("'.  Expected ");
        err.append(std::to_string(expectedMagic));
        err.append(".");
        throwRuntimeError(err);
      }
    }

    // header
    {
      v1::FileHeader header;
      SerializationTraits<decltype(header)>::read(in, header);

      // check kind
      if (header.kind != expectedKind)
      {
        std::string err;
        err.append("invalid file kind ");
        err.append(toString(header.kind));
        err.append(" for file '");
        err.append(filePath.string());
        err.append("'. Expected ");
        err.append(toString(expectedKind));
        err.append(".");
        throwRuntimeError(err);
      }
    }
  }

  /// Reads Magic and Header from a recording file trying with different versions in case of incompatibility
  static void tryReadMagicAndHeader(InputStream& in,
                                    const std::filesystem::path& filePath,
                                    uint32_t expectedMagic,
                                    v1::FileKind expectedKind)
  {
    try
    {
      readMagicAndHeaderV1(in, filePath, expectedMagic, expectedKind);
      return;
    }
    catch (...)
    {
      // NOTE try here with older versions if available
    }

    // throw error if no matching version of Magic and Header was found
    std::string err = "Invalid or unknown file version for '";
    err.append(filePath.string());
    err.append("'.");
    throwRuntimeError(err);
  }

  void tryReadSummary(InputStream& in)
  {
    try
    {
      SerializationTraits<decltype(summary_)>::read(in, summary_);
      return;
    }
    catch (...)
    {
      // NOTE: try here with older versions of the Summary struct
    }

    throwRuntimeError("Version of the sen::db::Summary struct in the input recording file cannot be found in sen::db.");
  }

  void tryReadTypes(InputStream& in)
  {
    const auto initialPosition = in.getPosition();
    kernel::CustomTypeSpecList recordingSpecs {};

    try
    {
      while (!in.atEnd())
      {
        recordingSpecs.emplace_back();
        auto& elem = recordingSpecs.back();
        SerializationTraits<kernel::CustomTypeSpec>::read(in, elem);
      }
    }
    catch (...)
    {
      recordingSpecs.clear();
      in.setPosition(initialPosition);

      try
      {
        while (!in.atEnd())
        {
          recordingSpecs.emplace_back();
          kernel::CustomTypeSpecV5 oldSpec {};
          SerializationTraits<kernel::CustomTypeSpecV5>::read(in, oldSpec);
          recordingSpecs.back() = toCurrentVersion(oldSpec);
        }
      }
      catch (...)
      {
        recordingSpecs.clear();
        in.setPosition(initialPosition);

        try
        {
          while (!in.atEnd())
          {
            recordingSpecs.emplace_back();
            kernel::CustomTypeSpecV4 oldSpec {};
            SerializationTraits<kernel::CustomTypeSpecV4>::read(in, oldSpec);
            recordingSpecs.back() = toCurrentVersion(oldSpec);
          }
        }
        catch (...)
        {
          getLogger()->error("Unknown or invalid type spec");
          throw std::runtime_error("Unknown or invalid type spec");
        }
      }
    }

    // add recording types to the kernel type set
    for (const auto& recordingSpec: recordingSpecs)
    {
      typesStorage_.emplace_back(buildNonNativeType(recordingSpec, kernelTypes_, {}));
      kernelTypes_.add(typesStorage_.back());
    }
  }

  void tryReadIndexes(InputStream& in)
  {
    while (!in.atEnd())
    {
      try
      {
        v1::IndexFileEntry entry;
        SerializationTraits<decltype(entry)>::read(in, entry);

        std::visit(Overloaded {[this](const v1::IndexDefinition& data)
                               {
                                 objectIndexDefinitions_.push_back(
                                   ObjectIndexDef {findClassAndAssociateObjectId(data.objectType, data.objectId),
                                                   data.objectId,
                                                   data.objectName,
                                                   data.session,
                                                   data.bus,
                                                   data.indexId});
                               },
                               [this](const v1::Index& data)
                               {
                                 if (data.indexId == keyframeIndexId)
                                 {
                                   keyframeIndexes_.push_back(KeyframeIndex {data.dataOffset, data.time});
                                 }
                                 else
                                 {
                                   objectIndexes_[data.indexId].push_back(ObjectIndex {data.dataOffset});
                                 }
                               }},
                   entry);
        continue;
      }
      catch (...)
      {
        // NOTE: try here with older versions of the v1::IndexFileEntry variant
      }

      throwRuntimeError("Version of the IndexFileEntry variant in the input recording file cannot be found.");
    }
  }

  [[nodiscard]] AnnotationCursor::Entry provideAnnotationFront(const AnnotationCursorState& state) const
  {
    auto* file = state.getFile();

    // do nothing if at the end
    if (const auto offset = static_cast<std::size_t>(ftell(file)); offset == state.getFileSize())
    {
      return {{}, End {}};
    }

    // read the entry
    v1::Annotation entry {};
    {
      std::vector<uint8_t> buffer;

      // read the entry size
      buffer.resize(SerializationTraits<uint32_t>::serializedSize(0U));
      if (fread(buffer.data(), 1U, buffer.size(), file) != buffer.size())
      {
        if (feof(file) != 0)
        {
          getLogger()->warn("Unexpected end-of-file while reading annotation information.");
          return {{}, End {}};
        }
        throwRuntimeError(strerror(errno));
      }

      uint32_t entrySize;
      {
        InputStream in(buffer);
        in.readUInt32(entrySize);
      }

      buffer.resize(entrySize);
      if (fread(buffer.data(), 1U, buffer.size(), file) != buffer.size())
      {
        if (feof(file) != 0)
        {
          getLogger()->warn("Unexpected end-of-file while reading annotation information.");
          return {{}, End {}};
        }
        throwRuntimeError(strerror(errno));
      }

      // read the data entry
      {
        InputStream in(buffer);
        SerializationTraits<v1::Annotation>::read(in, entry);
      }
    }

    // try to find the type
    const auto type = kernelTypes_.get(entry.type);

    SEN_ASSERT(type.has_value() && "The type for the annotation should be known by the kernel.");
    return {entry.time, db::Annotation(std::move(type).value(), std::move(entry.value).asVector())};
  }

  [[nodiscard]] DataCursor::Entry provideRuntimeDataFront(const DataCursorState& state)
  {
    auto* file = state.getFile();

    // do nothing if at the end
    if (const auto offset = static_cast<std::size_t>(ftell(file)); offset == state.getFileSize())
    {
      return {{}, End {}};
    }

    return readRuntimeDataEntry(file);
  }

  [[nodiscard]] DataCursor::Entry readRuntimeDataEntry(FILE* file)
  {
    v1::RuntimeDataEntry dataEntry {};
    {
      std::vector<uint8_t> buffer;

      // read the entry size
      buffer.resize(SerializationTraits<uint32_t>::serializedSize(0U));
      if (fread(buffer.data(), 1U, buffer.size(), file) != buffer.size())
      {
        // if we hit EOF or error, treat as end of recording (possibly truncated)
        if (feof(file) != 0)
        {
          getLogger()->warn("Unexpected end-of-file while reading runtime information.");
          return {{}, End {}};
        }
        throwRuntimeError(strerror(errno));
      }

      uint32_t entrySize;
      {
        InputStream in(buffer);
        in.readUInt32(entrySize);
      }

      buffer.resize(entrySize);
      if (fread(buffer.data(), 1U, buffer.size(), file) != buffer.size())
      {
        // If we hit EOF mid-entry, the recording was truncated here
        if (feof(file) != 0)
        {
          getLogger()->warn("Unexpected end-of-file while reading runtime information.");
          return {{}, End {}};
        }
        throwRuntimeError(strerror(errno));
      }

      // read the data entry
      {
        InputStream in(buffer);
        SerializationTraits<v1::RuntimeDataEntry>::read(in, dataEntry);
      }
    }

    auto processKeyframe = [this](v1::Keyframe& data) -> DataCursor::Entry
    {
      std::vector<Snapshot> snapshots;
      snapshots.reserve(data.objects.size());

      for (auto& elem: data.objects)
      {
        snapshots.push_back({elem.objectId,
                             elem.name,
                             elem.session,
                             elem.bus,
                             findClassAndAssociateObjectId(elem.type, elem.objectId),
                             std::move(elem.properties).asVector()});
      }

      return {data.time, db::Keyframe(std::move(snapshots))};
    };

    return std::visit(
      Overloaded {[this](v1::PropertyChange& data) -> DataCursor::Entry
                  {
                    const auto* classType = findClassForObjectId(data.objectId);
                    const auto* property = classType->searchPropertyById(data.propertyId);

                    // try to find property using platform compatible id
                    if (property == nullptr)
                    {
                      property = searchPlatformDependentProperty(data.propertyId, classType);

                      // throw an error if the property could not be found
                      if (property == nullptr)
                      {
                        std::string err;
                        {
                          err.append("property with ID '");
                          err.append(std::to_string(data.propertyId));
                          err.append("' not found in class '");
                          err.append(classType->getName());
                          err.append("' while reading property change from recording.");
                        }

                        getLogger()->error(err);
                        throwRuntimeError(err);
                      }
                    }

                    return {data.time, PropertyChange(data.objectId, property, std::move(data.value).asVector())};
                  },
                  [this](v1::Event& data) -> DataCursor::Entry
                  {
                    const auto* classType = findClassForObjectId(data.objectId);
                    const auto* event = classType->searchEventById(data.eventId);

                    // try to find property using platform compatible id
                    if (event == nullptr)
                    {
                      event = searchPlatformDependentEvent(data.eventId, classType);

                      // throw an error if the event could not be found
                      if (event == nullptr)
                      {
                        std::string err;
                        {
                          err.append("event with ID '");
                          err.append(std::to_string(data.eventId));
                          err.append("' not found in class '");
                          err.append(classType->getName());
                          err.append("' while reading event from recording.");
                        }

                        getLogger()->error(err);
                        throwRuntimeError(err);
                      }
                    }

                    return {data.time, db::Event(data.objectId, event, std::move(data.args).asVector())};
                  },
                  processKeyframe,
                  [&processKeyframe](v1::CompressedKeyframe& data) -> DataCursor::Entry
                  {
                    // uncompress the buffer
                    std::vector<uint8_t> memBuffer;
                    uncompressBuffer(data.buffer.asVector(), memBuffer, data.decompressedSize);

                    // read the keyframe
                    v1::Keyframe keyframe;
                    {
                      InputStream in(memBuffer);
                      SerializationTraits<v1::Keyframe>::read(in, keyframe);
                    }

                    return processKeyframe(keyframe);
                  },
                  [this](v1::Creation& data) -> DataCursor::Entry
                  {
                    return {data.time,
                            db::Creation(Snapshot(data.object.objectId,
                                                  data.object.name,
                                                  data.object.session,
                                                  data.object.bus,
                                                  findClassAndAssociateObjectId(data.object.type, data.object.objectId),
                                                  std::move(data.object.properties).asVector()))};
                  },
                  [](v1::Deletion& data) -> DataCursor::Entry { return {data.time, db::Deletion(data.objectId)}; }},
      dataEntry);
  }

  [[nodiscard]] ConstTypeHandle<ClassType> findClassAndAssociateObjectId(const std::string& typeName, ObjectId id)
  {
    auto type = kernelTypes_.get(typeName);

    if (!type)
    {
      std::string err;
      err.append("could not find class '");
      err.append(typeName);
      err.append("' for object with id ");
      err.append(std::to_string(id.get()));
      throwRuntimeError(err);
    }

    if (auto maybeClassType = dynamicTypeHandleCast<const ClassType>(std::move(type).value()))
    {
      objectClasses_.insert({id, maybeClassType->type()});
      return std::move(maybeClassType).value();
    }

    std::string err;
    err.append("found type '");
    err.append(typeName);
    err.append("' for object with id ");
    err.append(std::to_string(id.get()));
    err.append(", but it is not a class");
    throwRuntimeError(err);
  }

  [[nodiscard]] const ::sen::ClassType* findClassForObjectId(ObjectId id) const
  {
    auto itr = objectClasses_.find(id);
    if (itr == objectClasses_.end())
    {
      std::string err;
      err.append("could not find any class for object with id ");
      err.append(std::to_string(id.get()));
      throwRuntimeError(err);
    }

    return itr->second;
  }

  /// Searches properties in a class given their platform-dependent id. These platform-dependent IDs correspond to
  /// the old hash computation whose implementation was later changed to ensure connectivity between Sen processes in
  /// Windows and Linux platforms.
  const ::sen::Property* searchPlatformDependentProperty(u32 propertyId, const ::sen::ClassType* owner)
  {
    // check if the properties on this class have been cached
    if (const auto itr = platformDependentPropertyMap_.find(owner->getQualifiedName());
        itr != platformDependentPropertyMap_.end())
    {
      if (const auto it = itr->second.find(propertyId); it != itr->second.end())
      {
        return it->second;
      }

      return nullptr;
    }

    // store the properties of this class in the cache
    std::unordered_map<MemberHash, const ::sen::Property*> propertyMap;
    {
      for (const auto& prop: owner->getProperties(ClassType::SearchMode::includeParents))
      {
        propertyMap.emplace(computePlatformDependentPropertyId(owner, prop.get()), prop.get());
      }
    }

    platformDependentPropertyMap_.emplace(owner->getQualifiedName(), propertyMap);

    if (const auto itr = propertyMap.find(propertyId); itr != propertyMap.end())
    {
      return itr->second;
    }

    return nullptr;
  }

  /// Searches events in a class given their platform-dependent id. These platform-dependent IDs correspond to the
  /// old hash computation whose implementation was later changed to ensure connectivity between Sen processes in
  /// Windows and Linux platforms.
  const ::sen::Event* searchPlatformDependentEvent(u32 eventId, const ::sen::ClassType* owner)
  {
    // check if the events on this class have been cached
    if (const auto itr = platformDependentEventMap_.find(owner->getQualifiedName());
        itr != platformDependentEventMap_.end())
    {
      if (const auto it = itr->second.find(eventId); it != itr->second.end())
      {
        return it->second;
      }

      return nullptr;
    }

    // store the events of this class in the cache
    std::unordered_map<MemberHash, const ::sen::Event*> eventMap;
    {
      for (const auto& event: owner->getEvents(ClassType::SearchMode::includeParents))
      {
        eventMap.emplace(computePlatformDependentEventId(owner, event.get()), event.get());
      }
    }

    platformDependentEventMap_.emplace(owner->getQualifiedName(), eventMap);

    if (const auto itr = eventMap.find(eventId); itr != eventMap.end())
    {
      return itr->second;
    }

    return nullptr;
  }

private:
  std::filesystem::path path_;
  Summary summary_;
  CustomTypeRegistry& kernelTypes_;
  std::vector<ConstTypeHandle<CustomType>> typesStorage_;
  std::unordered_map<ObjectId, const ClassType*> objectClasses_;
  bool indexesRead_ = false;
  std::vector<ObjectIndexDef> objectIndexDefinitions_;
  std::vector<KeyframeIndex> keyframeIndexes_;
  std::unordered_map<ObjectId, ObjectIndexList> objectIndexes_;
  std::unordered_map<std::string_view, std::unordered_map<MemberHash, const ::sen::Property*>>
    platformDependentPropertyMap_;
  std::unordered_map<std::string_view, std::unordered_map<MemberHash, const ::sen::Event*>> platformDependentEventMap_;
};

//--------------------------------------------------------------------------------------------------------------
// Input
//--------------------------------------------------------------------------------------------------------------

Input::Input(std::filesystem::path path, CustomTypeRegistry& nativeTypes)
  : pimpl_(std::make_unique<Input::Impl>(std::move(path), nativeTypes))
{
}

Input::~Input() = default;

AnnotationCursor Input::annotationsBegin() { return pimpl_->annotationsBegin(); }

DataCursor Input::begin() { return pimpl_->begin(); }

DataCursor Input::at(const KeyframeIndex& index) { return pimpl_->at(index); }

DataCursor Input::makeCursor(const ObjectIndexDef& indexDef) { return pimpl_->makeCursor(indexDef); }

const std::filesystem::path& Input::getPath() const noexcept { return pimpl_->getPath(); }

const Summary& Input::getSummary() const noexcept { return pimpl_->getSummary(); }

const CustomTypeRegistry& Input::getTypes() const noexcept { return pimpl_->getTypes(); }

Span<const ObjectIndexDef> Input::getObjectIndexDefinitions() { return pimpl_->getObjectIndexDefinitions(); }

Span<const KeyframeIndex> Input::getAllKeyframeIndexes() { return pimpl_->getAllKeyframeIndexes(); }

std::optional<KeyframeIndex> Input::getKeyframeIndex(TimeStamp time)
{
  return pimpl_->getClosestKeyframeIndex(std::move(time));
}

void Input::addAnnotation(TimeStamp time, const Type* type, std::vector<uint8_t>&& value)
{
  pimpl_->addAnnotation(std::move(time), type, std::move(value));
}

}  // namespace sen::db
