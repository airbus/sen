// === output.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/output.h"

#include "stl/sen/kernel/basic_types.stl.h"

// implementation
#include "constants.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/type_specs_utils.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"
#include "v1.stl.h"

// Moody Camel's concurrent queue
#include "moodycamel/blockingconcurrentqueue.h"

// std
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <utility>
#include <variant>

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS  // NOLINT
#  pragma warning(disable: 4996)   // NOLINT
#endif

namespace sen::db
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// constants
//--------------------------------------------------------------------------------------------------------------

constexpr std::size_t workerBulkSize = 128U;
constexpr std::size_t snapshotPropertiesBaseSize = 512U;

//--------------------------------------------------------------------------------------------------------------
// Thread commands
//--------------------------------------------------------------------------------------------------------------

struct Stop
{
};

struct WriteData
{
  FILE* dataFile = nullptr;
  std::shared_ptr<ResizableHeapBlock> data;
};

struct WriteDataAndIndex
{
  FILE* dataFile = nullptr;
  std::shared_ptr<ResizableHeapBlock> data;
  FILE* indexesFile = nullptr;
  TimeStamp time;
  ObjectId id;
};

using ThreadCommand = std::variant<Stop, WriteData, WriteDataAndIndex>;

//--------------------------------------------------------------------------------------------------------------
// Queue
//--------------------------------------------------------------------------------------------------------------

class Queue
{
  SEN_NOCOPY_NOMOVE(Queue)

public:
  Queue() = default;
  ~Queue() = default;

public:
  void enqueue(Stop&& cmd) { queue_.enqueue(std::move(cmd)); }

  void enqueue(WriteData&& data)
  {
    const auto size = data.data->size();
    queue_.enqueue(std::move(data));

    sizeBytes_ += size;
    maxSize_ = std::max(maxSize_.load(), sizeBytes_.load());
  }

  void enqueue(WriteDataAndIndex&& data)
  {
    const auto size = data.data->size();
    queue_.enqueue(data);
    sizeBytes_ += size;
  }

  void markConsumed(size_t size) { sizeBytes_ -= size; }

  [[nodiscard]] uint64_t getMaxSize() const { return maxSize_.load(); }

  template <typename It>
  [[nodiscard]] size_t dequeue(It itemFirst, size_t max)
  {
    return queue_.wait_dequeue_bulk(itemFirst, max);
  }

private:
  moodycamel::BlockingConcurrentQueue<ThreadCommand> queue_;
  std::atomic_uint64_t sizeBytes_ = 0U;
  std::atomic_uint64_t maxSize_ = 0U;
};

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

FILE* createFile(const std::filesystem::path& filePath)
{
  auto* file = fopen(filePath.string().c_str(), "wb");  // NOLINT
  if (file == nullptr)
  {
    auto* description = strerror(errno);
    getLogger()->error("could not create file '{}'. {}", filePath.string(), description);

    std::string err;
    err.append("could not create file '");
    err.append(filePath.string());
    err.append("'. ");
    err.append(description);
    throwRuntimeError(err);
  }

  return file;
}

void setSnapshot(const ObjectInfo& info, v1::ObjectSnapshot& snapshot)
{
  // basic data
  snapshot.name = info.instance->getName();
  snapshot.objectId = info.instance->getId().get();
  snapshot.type = info.instance->getClass()->getQualifiedName();
  snapshot.session = info.session;
  snapshot.bus = info.bus;

  // properties
  snapshot.properties.reserve(snapshotPropertiesBaseSize);
  ResizableBufferWriter writer(snapshot.properties);
  OutputStream out(writer);
  ::sen::impl::writeAllPropertiesToStream(info.instance, out);
}

//--------------------------------------------------------------------------------------------------------------
// Archive
//--------------------------------------------------------------------------------------------------------------

class Archive
{
public:
  SEN_MOVE_ONLY(Archive)

public:
  explicit Archive(const std::filesystem::path& path)
    : dataFile_(createFile(path / runtimeFileName))
    , indexesFile_(createFile(path / indexesFileName))
    , typesFile_(createFile(path / typesFileName))
    , annotationsFile_(createFile(path / annotationsFileName))
    , summaryFile_(createFile(path / summaryFileName))
    , files_ {dataFile_, indexesFile_, annotationsFile_, typesFile_, summaryFile_}
  {
    // magic
    std::for_each(files_.cbegin(), files_.cend(), [](auto& file) { write<v1::Magic>({beginMagic}, file); });

    // headers
    constexpr uint32_t version = 1U;
    write<v1::FileHeader>({version, v1::FileKind::dataFile}, dataFile_);
    write<v1::FileHeader>({version, v1::FileKind::indexesFile}, indexesFile_);
    write<v1::FileHeader>({version, v1::FileKind::typesFile}, typesFile_);
    write<v1::FileHeader>({version, v1::FileKind::annotationsFile}, annotationsFile_);
    write<v1::FileHeader>({version, v1::FileKind::summaryFile}, summaryFile_);

    // flush headers to disk immediately for crash resilience
    std::for_each(files_.cbegin(), files_.cend(), [](auto& file) { fflush(file); });

    // summary
    summary_.firstTime = TimeStamp(Duration(std::numeric_limits<Duration::ValueType>::max()));
    summary_.lastTime = TimeStamp(Duration(std::numeric_limits<Duration::ValueType>::min()));
    summary_.keyframeCount = 0U;
    summary_.objectCount = 0U;
    summary_.typeCount = 0U;
    summary_.annotationCount = 0U;
    summary_.indexedObjectCount = 0U;

    // write initial summary for crash resilience
    updateSummary();
  }

  ~Archive()
  {
    try
    {
      // final summary update before closing
      updateSummary();
    }
    catch (...)
    {
      // no code needed
    }

    std::for_each(files_.cbegin(), files_.cend(), [](auto& file) { closeFile(file); });
  }

public:
  void keyframe(Queue& queue, TimeStamp time, const ObjectInfoList& objects, bool createIndex)
  {
    auto key = makeKeyframe(time, objects);

    if (compressKeyframes_)
    {
      ::sen::kernel::Buffer memBuf;
      writeToBuffer(key, memBuf);

      v1::CompressedKeyframe compressedKey;
      compressedKey.decompressedSize = static_cast<uint32_t>(memBuf.size());
      writeToCompressedBuffer(memBuf, compressedKey.buffer);

      if (createIndex)
      {
        writeIndexedRuntimeData(queue, std::move(compressedKey), time, keyframeIndexId);
      }
      else
      {
        writeRuntimeData(queue, std::move(compressedKey));
      }
    }
    else
    {
      if (createIndex)
      {
        writeIndexedRuntimeData(queue, std::move(key), time, keyframeIndexId);
      }
      else
      {
        writeRuntimeData(queue, std::move(key));
      }
    }

    // update summary after each keyframe for crash resilience
    updateSummary();
  }

  void creation(Queue& queue, TimeStamp time, const ObjectInfo& object, bool indexed)
  {
    updateTimeStats(time);
    summary_.objectCount++;

    v1::Creation data {time, {}};
    setSnapshot(object, data.object);

    if (indexed)
    {
      const auto id = object.instance->getId();
      v1::IndexDefinition indexDef {id.get(),
                                    id.get(),
                                    object.session,
                                    object.bus,
                                    object.instance->getName(),
                                    std::string(object.instance->getClass()->getQualifiedName())};

      indexedObjects_.insert(id);

      // index definition
      writeToFile(queue, v1::IndexFileEntry {std::move(indexDef)}, indexesFile_);

      // creation and first index
      writeIndexedRuntimeData(queue, std::move(data), time, id.get());

      summary_.indexedObjectCount++;
    }
    else
    {
      // creation
      writeRuntimeData(queue, std::move(data));
    }

    // deal with the type, if not done before
    ensureTypeAndSubtypesWritten(object.instance->getClass().type());
  }

  void deletion(Queue& queue, TimeStamp time, ObjectId objectId)
  {
    updateTimeStats(time);
    v1::Deletion data {time, objectId.get()};

    if (indexedObjects_.count(objectId) != 0U)
    {
      // this is a deletion, so we stop tracking this object
      indexedObjects_.erase(objectId);
      writeIndexedRuntimeData(queue, std::move(data), time, objectId.get());
    }
    else
    {
      writeRuntimeData(queue, std::move(data));
    }
  }

  void propertyChange(Queue& queue,
                      TimeStamp time,
                      ObjectId objectId,
                      MemberHash propertyId,
                      ::sen::kernel::Buffer&& propertyValue)
  {
    updateTimeStats(time);

    v1::PropertyChange data {time, objectId.get(), propertyId.get(), std::move(propertyValue)};

    if (indexedObjects_.count(objectId) != 0U)
    {
      writeIndexedRuntimeData(queue, std::move(data), time, objectId.get());
    }
    else
    {
      writeRuntimeData(queue, std::move(data));
    }
  }

  void event(Queue& queue, TimeStamp time, ObjectId objectId, MemberHash eventId, ::sen::kernel::Buffer&& args)
  {
    updateTimeStats(time);

    v1::Event data {time, objectId.get(), eventId.get(), std::move(args)};

    if (indexedObjects_.count(objectId) != 0U)
    {
      writeIndexedRuntimeData(queue, std::move(data), time, objectId.get());
    }
    else
    {
      writeRuntimeData(queue, std::move(data));
    }
  }

  void annotation(Queue& queue, TimeStamp time, const Type* type, ::sen::kernel::Buffer&& value)
  {
    updateTimeStats(time);
    summary_.annotationCount++;

    v1::Annotation data {time, std::string(type->getName()), std::move(value)};

    if (const auto* custom = type->asCustomType(); custom)
    {
      data.type = std::string(custom->getQualifiedName());
      ensureTypeAndSubtypesWritten(custom);
    }

    auto buffer = writeSizeAndDataToBuffer(data);
    queue.enqueue(WriteData {annotationsFile_, std::move(buffer)});
  }

private:
  void ensureTypeAndSubtypesWritten(const CustomType* type)
  {
    SEN_ASSERT(type != nullptr && "A type is required for writing the output.");

    // only deal with this type if not done so before
    if (writtenTypes_.count(type) == 0)
    {
      bool typesWritten = false;

      std::function<void(ConstTypeHandle<>)> callback = [this, &typesWritten](ConstTypeHandle<> dependent)
      {
        if (const auto custom = dependent->asCustomType(); custom != nullptr)
        {
          auto customType = custom;

          if (writtenTypes_.count(customType) == 0U)
          {
            auto spec = ::sen::kernel::makeCustomTypeSpec(custom);
            write(std::move(spec), typesFile_);
            summary_.typeCount++;
            writtenTypes_.insert(customType);
            typesWritten = true;
          }
        }
      };

      // write all the dependent custom types that are not present
      iterateOverDependentTypes(*type, callback);

      // write the main type synchronously
      write(::sen::kernel::makeCustomTypeSpec(type), typesFile_);
      writtenTypes_.insert(type);
      typesWritten = true;

      // flush to ensure types are on disk before runtime data is written
      if (typesWritten)
      {
        fflush(typesFile_);
      }
    }
  }

  v1::Keyframe makeKeyframe(const TimeStamp& time, const ObjectInfoList& objects)
  {
    updateTimeStats(time);
    summary_.keyframeCount++;

    v1::Keyframe data {time, {}};
    data.objects.reserve(objects.size());

    for (const auto& elem: objects)
    {
      setSnapshot(elem, data.objects.emplace_back());
    }

    return data;
  }

  template <typename T>
  SEN_ALWAYS_INLINE void writeToFile(Queue& queue, T&& data, FILE* file) const
  {
    auto buf = std::make_shared<ResizableHeapBlock>();
    writeToBuffer(data, *buf);

    queue.enqueue(WriteData {file, std::move(buf)});
  }

  template <typename T>
  SEN_ALWAYS_INLINE void writeRuntimeData(Queue& queue, T&& data)
  {
    queue.enqueue(WriteData {dataFile_, writeSizeAndDataToBuffer(v1::RuntimeDataEntry {data})});
  }

  template <typename T>
  SEN_ALWAYS_INLINE void writeIndexedRuntimeData(Queue& queue, T&& data, TimeStamp time, uint32_t indexId)
  {
    queue.enqueue(WriteDataAndIndex {
      dataFile_, writeSizeAndDataToBuffer(v1::RuntimeDataEntry {data}), indexesFile_, time, indexId});
  }

  SEN_ALWAYS_INLINE void updateTimeStats(const TimeStamp& time)
  {
    summary_.lastTime = std::max(time, summary_.lastTime);
    summary_.firstTime = std::min(time, summary_.firstTime);
  }

  void updateSummary() const
  {
    fseek(summaryFile_, static_cast<std::int64_t>(getFileHeaderSize()), SEEK_SET);
    write<Summary>(Summary {summary_}, summaryFile_);
    fflush(summaryFile_);
  }

  static void closeFile(FILE* file) noexcept
  {
    if (file == nullptr)
    {
      return;
    }

    std::ignore = fflush(file);  // NOLINT
    std::ignore = fclose(file);  // NOLINT
  }

private:
  FILE* dataFile_;
  FILE* indexesFile_;
  FILE* typesFile_;
  FILE* annotationsFile_;
  FILE* summaryFile_;
  std::array<FILE*, 5U> files_;  // NOLINT
  std::unordered_set<ObjectId> indexedObjects_;
  std::unordered_set<const CustomType*> writtenTypes_;
  Summary summary_ {};
  bool compressKeyframes_ = true;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Output::Impl
//--------------------------------------------------------------------------------------------------------------

class Output::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl(OutSettings settings, ErrorHandler errorHandler)
    : settings_(std::move(settings)), errorHandler_(std::move(errorHandler))
  {
    // folder where all our archive files will reside
    std::filesystem::path targetPath = settings_.folder.empty()
                                         ? std::filesystem::path(settings_.name)
                                         : std::filesystem::path(settings_.folder) / settings_.name;

    // append a number until we find a path that is available
    std::size_t count = 1U;
    const auto basePath = targetPath;
    while (std::filesystem::exists(targetPath))
    {
      targetPath = basePath.string() + "_" + std::to_string(count);
      count++;
    }

    // create the target directory
    if (std::error_code err; !std::filesystem::create_directories(targetPath, err))
    {
      throwRuntimeError(err.message());
    }

    // try to initialize the files
    archive_ = std::make_unique<Archive>(targetPath);

    // start the worker
    thread_ = std::thread(
      [this]()
      {
        try
        {
          workerThread();
        }
        catch (...)
        {
          errorHandler_();
        }
      });
  }

  ~Impl()
  {
    // stop the worker thread
    if (thread_.joinable())
    {
      // signal the thread to stop
      queue_.enqueue(Stop {});

      // wait until it stops
      thread_.join();
    }
  }

public:
  [[nodiscard]] const OutSettings& getSettings() const noexcept { return settings_; }

  [[nodiscard]] const TimeStamp& getCreationTime() const noexcept { return creationTime_; }

  [[nodiscard]] OutStats fetchStats() const
  {
    OutStats result;
    result.maxQueueSize = queue_.getMaxSize();
    return result;
  }

public:
  void keyframe(TimeStamp time, const ObjectInfoList& objects)
  {
    archive_->keyframe(queue_, time, objects, settings_.indexKeyframes);
  }

  void creation(TimeStamp time, const ObjectInfo& object, bool indexed)
  {
    archive_->creation(queue_, time, object, indexed);
  }

  void deletion(TimeStamp time, ObjectId objectId) { archive_->deletion(queue_, time, objectId); }

  void propertyChange(TimeStamp time, ObjectId objectId, MemberHash propertyId, ::sen::kernel::Buffer&& propertyValue)
  {
    archive_->propertyChange(queue_, time, objectId, propertyId, std::move(propertyValue));
  }

  void event(TimeStamp time, ObjectId objectId, MemberHash eventId, ::sen::kernel::Buffer&& args)
  {
    archive_->event(queue_, time, objectId, eventId, std::move(args));
  }

  void annotation(TimeStamp time, const Type* type, ::sen::kernel::Buffer&& value)
  {
    archive_->annotation(queue_, time, type, std::move(value));
  }

private:
  void workerThread()
  {
    // the batch of commands we will work with
    std::array<ThreadCommand, workerBulkSize> batch;

    enum class OpResult
    {
      stop,
      doNotStop
    };

    // do work
    OpResult result = OpResult::doNotStop;

    while (result != OpResult::stop)
    {
      // get work
      auto count = queue_.dequeue(batch.begin(), batch.max_size());

      for (std::size_t i = 0U; i < count; ++i)
      {
        if (result == OpResult::stop)
        {
          break;
        }

        // execute the command
        result = std::visit(Overloaded {[](const Stop& /*cmd*/) { return OpResult::stop; },
                                        [this](const WriteData& cmd)
                                        {
                                          // write our new entry
                                          auto span = cmd.data->getConstSpan();
                                          doWrite(span, cmd.dataFile);
                                          queue_.markConsumed(span.size());

                                          return OpResult::doNotStop;
                                        },
                                        [this](const WriteDataAndIndex& cmd)
                                        {
                                          // get the offset of our new entry
                                          auto dataOffset = static_cast<uint64_t>(ftell(cmd.dataFile));

                                          // write our new entry
                                          auto span = cmd.data->getConstSpan();
                                          doWrite(span, cmd.dataFile);
                                          queue_.markConsumed(span.size());

                                          // write the index entry
                                          v1::IndexFileEntry entry {v1::Index {cmd.time, cmd.id.get(), dataOffset}};
                                          write<decltype(entry)>(std::move(entry), cmd.indexesFile);

                                          return OpResult::doNotStop;
                                        }},
                            batch[i]);  // NOLINT

        // clear the command
        batch[i] = {};  // NOLINT
      }
    }
  }

private:
  Queue queue_;
  std::thread thread_;
  std::unique_ptr<Archive> archive_;
  OutSettings settings_;
  TimeStamp creationTime_;
  ErrorHandler errorHandler_;
};

//--------------------------------------------------------------------------------------------------------------
// Output
//--------------------------------------------------------------------------------------------------------------

Output::Output(OutSettings settings, ErrorHandler errorHandler)
  : pimpl_(std::make_unique<Output::Impl>(std::move(settings), std::move(errorHandler)))
{
}

Output::~Output() = default;

void Output::keyframe(TimeStamp time, const ObjectInfoList& objects) { pimpl_->keyframe(time, objects); }

void Output::creation(TimeStamp time, const ObjectInfo& object, bool indexed)
{
  pimpl_->creation(time, object, indexed);
}

void Output::deletion(TimeStamp time, ObjectId objectId) { pimpl_->deletion(time, objectId); }

void Output::propertyChange(TimeStamp time,
                            ObjectId objectId,
                            MemberHash propertyId,
                            ::sen::kernel::Buffer&& propertyValue)
{
  pimpl_->propertyChange(time, objectId, propertyId, std::move(propertyValue));
}

void Output::event(TimeStamp time, ObjectId objectId, MemberHash eventId, ::sen::kernel::Buffer&& args)
{
  pimpl_->event(time, objectId, eventId, std::move(args));
}

void Output::annotation(TimeStamp time, const Type* type, ::sen::kernel::Buffer&& value)
{
  pimpl_->annotation(time, type, std::move(value));
}

OutStats Output::fetchStats() const { return pimpl_->fetchStats(); }

const OutSettings& Output::getSettings() const noexcept { return pimpl_->getSettings(); }

const TimeStamp& Output::getCreationTime() const noexcept { return pimpl_->getCreationTime(); }

}  // namespace sen::db
