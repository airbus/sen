// === recording_merger_test.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// test
#include "db_test_helpers.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/native_types.h"
#include "sen/db/annotation.h"
#include "sen/db/creation.h"
#include "sen/db/deletion.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/output.h"
#include "sen/db/property_change.h"
#include "sen/db/recording_merger.h"
#include "sen/kernel/test_kernel.h"
#include "sen/test_support/archive_test_helpers.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace sen::db::test
{

namespace
{

using sen::test::makeArchivePath;
using sen::test::makeArchiveSettings;
using sen::test::makeTime;
using sen::test::TempDir;

struct RecordedAnnotation
{
  TimeStamp time;
  uint8_t value = 0U;
};

void writeKeyframeRecording(std::string_view name, const TempDir& tempDir, std::vector<TimeStamp> times)
{
  auto settings = makeArchiveSettings(name, tempDir, false);
  Output output(std::move(settings), []() {});
  for (const auto& time: times)
  {
    output.keyframe(time, {});
  }
}

void writeIndexedCreationRecording(std::string_view name,
                                   const TempDir& tempDir,
                                   const ObjectInfo& object,
                                   TimeStamp time)
{
  auto settings = makeArchiveSettings(name, tempDir);
  Output output(std::move(settings), []() {});
  output.creation(std::move(time), object, true);
}

[[nodiscard]] ::sen::kernel::Buffer makeFloat64Buffer(float64_t value)
{
  ::sen::kernel::Buffer buffer;
  sen::ResizableBufferWriter writer(buffer);
  sen::OutputStream out(writer);
  sen::SerializationTraits<float64_t>::write(out, value);
  return buffer;
}

void writeIndexedLifecycleRecording(std::string_view name,
                                    const TempDir& tempDir,
                                    const ObjectInfo& object,
                                    TimeStamp creationTime,
                                    TimeStamp propertyChangeTime,
                                    TimeStamp deletionTime,
                                    float64_t propertyValue)
{
  auto settings = makeArchiveSettings(name, tempDir);
  Output output(std::move(settings), []() {});

  const auto properties = object.instance->getClass()->getProperties(sen::ClassType::SearchMode::includeParents);
  ASSERT_FALSE(properties.empty());

  output.creation(std::move(creationTime), object, true);
  output.propertyChange(std::move(propertyChangeTime),
                        object.instance->getId(),
                        properties.front()->getId(),
                        makeFloat64Buffer(propertyValue));
  output.deletion(std::move(deletionTime), object.instance->getId());
}

void writeAnnotationRecording(std::string_view name,
                              const TempDir& tempDir,
                              const std::vector<RecordedAnnotation>& annotations)
{
  const auto typeInt32 = sen::Int32Type::get();
  auto settings = makeArchiveSettings(name, tempDir, false);
  Output output(std::move(settings), []() {});

  for (const auto& annotation: annotations)
  {
    ::sen::kernel::Buffer value;
    value.push_back(annotation.value);
    value.push_back(0U);
    value.push_back(0U);
    value.push_back(0U);
    output.annotation(annotation.time, typeInt32.type(), std::move(value));
  }
}

[[nodiscard]] RecordingMergeInput makeMergeInput(std::filesystem::path archivePath, Duration offset = {})
{
  RecordingMergeInput input;
  input.archivePath = std::move(archivePath);
  input.offset = offset;

  return input;
}

[[nodiscard]] RecordingMergeSettings makeMergeSettings(std::vector<std::filesystem::path> inputArchives,
                                                       std::filesystem::path outputArchive)
{
  RecordingMergeSettings settings;
  settings.outputArchive = std::move(outputArchive);
  settings.inputArchives.reserve(inputArchives.size());
  for (auto& inputArchive: inputArchives)
  {
    settings.inputArchives.push_back(makeMergeInput(std::move(inputArchive)));
  }

  return settings;
}

}  // namespace

/// @test
/// Quickly validates that several recordings can be merged by runtime timestamp ordering.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, RawMergeOrdersRuntimeEntriesChronologically)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  writeKeyframeRecording("first", tempDir, {makeTime(30), makeTime(10)});
  writeKeyframeRecording("second", tempDir, {makeTime(20), makeTime(40)});

  const auto mergedArchive = makeArchivePath("merged", tempDir);
  auto settings =
    makeMergeSettings({makeArchivePath("first", tempDir), makeArchivePath("second", tempDir)}, mergedArchive);
  mergeRecordings(settings);

  Input input(mergedArchive, kernel.getTypes());
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.firstTime, makeTime(10));
  EXPECT_EQ(summary.lastTime, makeTime(40));
  EXPECT_EQ(summary.keyframeCount, 4U);
  EXPECT_EQ(summary.objectCount, 0U);
  EXPECT_EQ(summary.typeCount, 0U);
  EXPECT_EQ(summary.annotationCount, 0U);
  EXPECT_EQ(summary.indexedObjectCount, 0U);

  auto keyframes = input.getAllKeyframeIndexes();
  ASSERT_EQ(keyframes.size(), 4U);
  EXPECT_EQ(keyframes[0].time, makeTime(10));
  EXPECT_EQ(keyframes[1].time, makeTime(20));
  EXPECT_EQ(keyframes[2].time, makeTime(30));
  EXPECT_EQ(keyframes[3].time, makeTime(40));

  std::vector<TimeStamp> entryTimes;
  auto cursor = input.begin();
  while (!cursor.atEnd())
  {
    ++cursor;
    if (!cursor.atEnd())
    {
      entryTimes.push_back(cursor.get().time);
    }
  }

  ASSERT_EQ(entryTimes.size(), 4U);
  EXPECT_EQ(entryTimes[0], makeTime(10));
  EXPECT_EQ(entryTimes[1], makeTime(20));
  EXPECT_EQ(entryTimes[2], makeTime(30));
  EXPECT_EQ(entryTimes[3], makeTime(40));
}

/// @test
/// Verifies that zero-aligned merges move every input recording to its own start time.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeZeroAlignsRuntimeEntries)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  writeKeyframeRecording("first", tempDir, {makeTime(30), makeTime(40)});
  writeKeyframeRecording("second", tempDir, {makeTime(100), makeTime(120)});

  RecordingMergeSettings settings;
  settings.mode = RecordingMergeMode::zeroAligned;
  settings.outputArchive = makeArchivePath("merged_zero_aligned", tempDir);
  settings.inputArchives = {makeMergeInput(makeArchivePath("first", tempDir)),
                            makeMergeInput(makeArchivePath("second", tempDir))};
  mergeRecordings(settings);

  Input input(settings.outputArchive, kernel.getTypes());
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.firstTime, makeTime(0));
  EXPECT_EQ(summary.lastTime, makeTime(20));
  EXPECT_EQ(summary.keyframeCount, 4U);

  auto keyframes = input.getAllKeyframeIndexes();
  ASSERT_EQ(keyframes.size(), 4U);
  EXPECT_EQ(keyframes[0].time, makeTime(0));
  EXPECT_EQ(keyframes[1].time, makeTime(0));
  EXPECT_EQ(keyframes[2].time, makeTime(10));
  EXPECT_EQ(keyframes[3].time, makeTime(20));
}

/// @test
/// Verifies that offset-aligned merges apply an independent start offset to every input recording.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeAppliesRuntimeOffsets)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  writeKeyframeRecording("first", tempDir, {makeTime(10), makeTime(20)});
  writeKeyframeRecording("second", tempDir, {makeTime(100), makeTime(110)});

  RecordingMergeSettings settings;
  settings.mode = RecordingMergeMode::offsetAligned;
  settings.outputArchive = makeArchivePath("merged_with_offsets", tempDir);
  settings.inputArchives = {makeMergeInput(makeArchivePath("first", tempDir), Duration(std::chrono::seconds(5))),
                            makeMergeInput(makeArchivePath("second", tempDir), Duration(std::chrono::seconds(30)))};
  mergeRecordings(settings);

  Input input(settings.outputArchive, kernel.getTypes());
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.firstTime, makeTime(5));
  EXPECT_EQ(summary.lastTime, makeTime(40));
  EXPECT_EQ(summary.keyframeCount, 4U);

  auto keyframes = input.getAllKeyframeIndexes();
  ASSERT_EQ(keyframes.size(), 4U);
  EXPECT_EQ(keyframes[0].time, makeTime(5));
  EXPECT_EQ(keyframes[1].time, makeTime(15));
  EXPECT_EQ(keyframes[2].time, makeTime(30));
  EXPECT_EQ(keyframes[3].time, makeTime(40));
}

/// @test
/// Verifies that offset-aligned merges also shift object creation, property change, and deletion entries.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeAppliesRuntimeOffsetsToObjectEntries)
{
  TempDir tempDir;
  DualClassSetup setup;

  writeIndexedLifecycleRecording(
    "first", tempDir, makeObjectInfo(setup.testObject), makeTime(100), makeTime(110), makeTime(120), 100.0);
  writeIndexedLifecycleRecording(
    "second", tempDir, makeObjectInfo(setup.otherObject), makeTime(200), makeTime(210), makeTime(220), 200.0);

  RecordingMergeSettings settings;
  settings.mode = RecordingMergeMode::offsetAligned;
  settings.outputArchive = makeArchivePath("merged_object_entries_with_offsets", tempDir);
  settings.inputArchives = {makeMergeInput(makeArchivePath("first", tempDir), Duration(std::chrono::seconds(5))),
                            makeMergeInput(makeArchivePath("second", tempDir), Duration(std::chrono::seconds(30)))};
  mergeRecordings(settings);

  Input input(settings.outputArchive, setup.kernel->getTypes());
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.firstTime, makeTime(5));
  EXPECT_EQ(summary.lastTime, makeTime(50));
  EXPECT_EQ(summary.keyframeCount, 0U);
  EXPECT_EQ(summary.objectCount, 2U);
  EXPECT_EQ(summary.typeCount, 2U);
  EXPECT_EQ(summary.annotationCount, 0U);
  EXPECT_EQ(summary.indexedObjectCount, 2U);

  std::vector<TimeStamp> creationTimes;
  std::vector<TimeStamp> propertyChangeTimes;
  std::vector<TimeStamp> deletionTimes;
  std::set<uint32_t> creationObjectIds;
  std::set<uint32_t> propertyChangeObjectIds;
  std::set<uint32_t> deletionObjectIds;

  auto cursor = input.begin();
  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Creation>(entry.payload))
    {
      const auto& creation = std::get<Creation>(entry.payload);
      creationTimes.push_back(entry.time);
      creationObjectIds.insert(creation.getSnapshot().getObjectId().get());
    }
    else if (std::holds_alternative<PropertyChange>(entry.payload))
    {
      const auto& propertyChange = std::get<PropertyChange>(entry.payload);
      propertyChangeTimes.push_back(entry.time);
      propertyChangeObjectIds.insert(propertyChange.getObjectId().get());
    }
    else if (std::holds_alternative<Deletion>(entry.payload))
    {
      const auto& deletion = std::get<Deletion>(entry.payload);
      deletionTimes.push_back(entry.time);
      deletionObjectIds.insert(deletion.getObjectId().get());
    }
  }

  ASSERT_EQ(creationTimes.size(), 2U);
  EXPECT_EQ(creationTimes[0], makeTime(5));
  EXPECT_EQ(creationTimes[1], makeTime(30));

  ASSERT_EQ(propertyChangeTimes.size(), 2U);
  EXPECT_EQ(propertyChangeTimes[0], makeTime(15));
  EXPECT_EQ(propertyChangeTimes[1], makeTime(40));

  ASSERT_EQ(deletionTimes.size(), 2U);
  EXPECT_EQ(deletionTimes[0], makeTime(25));
  EXPECT_EQ(deletionTimes[1], makeTime(50));

  EXPECT_EQ(propertyChangeObjectIds, creationObjectIds);
  EXPECT_EQ(deletionObjectIds, creationObjectIds);
}

/// @test
/// Verifies that the merged archive rebuilds object indexes with offsets into the merged runtime file.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeRebuildsObjectIndexes)
{
  TempDir tempDir;
  DualClassSetup setup;

  writeIndexedLifecycleRecording(
    "first", tempDir, makeObjectInfo(setup.testObject), makeTime(10), makeTime(15), makeTime(25), 100.0);
  writeIndexedLifecycleRecording(
    "second", tempDir, makeObjectInfo(setup.otherObject), makeTime(20), makeTime(30), makeTime(40), 200.0);

  const auto mergedArchive = makeArchivePath("merged_with_indexes", tempDir);
  auto settings =
    makeMergeSettings({makeArchivePath("first", tempDir), makeArchivePath("second", tempDir)}, mergedArchive);
  mergeRecordings(settings);

  Input input(mergedArchive, setup.kernel->getTypes());
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.firstTime, makeTime(10));
  EXPECT_EQ(summary.lastTime, makeTime(40));
  EXPECT_EQ(summary.keyframeCount, 0U);
  EXPECT_EQ(summary.objectCount, 2U);
  EXPECT_EQ(summary.typeCount, 2U);
  EXPECT_EQ(summary.annotationCount, 0U);
  EXPECT_EQ(summary.indexedObjectCount, 2U);

  auto indexedObjects = input.getObjectIndexDefinitions();
  ASSERT_EQ(indexedObjects.size(), 2U);

  std::set<std::string> indexedObjectNames;
  for (const auto& indexedObject: indexedObjects)
  {
    indexedObjectNames.insert(indexedObject.name);

    const auto expectedCreationTime = indexedObject.name == "testObj" ? makeTime(10) : makeTime(20);
    const auto expectedPropertyTime = indexedObject.name == "testObj" ? makeTime(15) : makeTime(30);
    const auto expectedDeletionTime = indexedObject.name == "testObj" ? makeTime(25) : makeTime(40);

    auto cursor = input.makeCursor(indexedObject);
    ++cursor;
    ASSERT_FALSE(cursor.atEnd());
    ASSERT_TRUE(std::holds_alternative<Creation>(cursor.get().payload));
    EXPECT_EQ(cursor.get().time, expectedCreationTime);

    const auto& creation = std::get<Creation>(cursor.get().payload);
    EXPECT_EQ(creation.getSnapshot().getName(), indexedObject.name);
    EXPECT_EQ(creation.getSnapshot().getObjectId(), indexedObject.objectId);

    ++cursor;
    ASSERT_FALSE(cursor.atEnd());
    ASSERT_TRUE(std::holds_alternative<PropertyChange>(cursor.get().payload));
    EXPECT_EQ(cursor.get().time, expectedPropertyTime);

    const auto& propertyChange = std::get<PropertyChange>(cursor.get().payload);
    EXPECT_EQ(propertyChange.getObjectId(), indexedObject.objectId);
    EXPECT_NE(propertyChange.getProperty(), nullptr);

    ++cursor;
    ASSERT_FALSE(cursor.atEnd());
    ASSERT_TRUE(std::holds_alternative<Deletion>(cursor.get().payload));
    EXPECT_EQ(cursor.get().time, expectedDeletionTime);

    const auto& deletion = std::get<Deletion>(cursor.get().payload);
    EXPECT_EQ(deletion.getObjectId(), indexedObject.objectId);

    ++cursor;
    EXPECT_TRUE(cursor.atEnd());
  }

  EXPECT_TRUE(indexedObjectNames.count("testObj") != 0U);
  EXPECT_TRUE(indexedObjectNames.count("otherObj") != 0U);
}

/// @test
/// Verifies that object ids from different input recordings are remapped into the merged archive id space.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeRemapsObjectIdsFromDifferentRecordings)
{
  TempDir tempDir;
  SingleClassSetup setup;

  writeIndexedCreationRecording(
    "first", tempDir, makeObjectInfo(setup.object, "first_session", "first_bus"), makeTime(10));
  writeIndexedCreationRecording(
    "second", tempDir, makeObjectInfo(setup.object, "second_session", "second_bus"), makeTime(20));

  const auto mergedArchive = makeArchivePath("merged_with_remapped_object_ids", tempDir);
  auto settings =
    makeMergeSettings({makeArchivePath("first", tempDir), makeArchivePath("second", tempDir)}, mergedArchive);
  mergeRecordings(settings);

  Input input(mergedArchive, setup.kernel->getTypes());

  std::set<uint32_t> objectIds;
  std::set<std::string> busNames;
  auto cursor = input.begin();
  while (!cursor.atEnd())
  {
    ++cursor;
    if (!cursor.atEnd() && std::holds_alternative<Creation>(cursor.get().payload))
    {
      const auto& creation = std::get<Creation>(cursor.get().payload);
      objectIds.insert(creation.getSnapshot().getObjectId().get());
      busNames.insert(creation.getSnapshot().getBusName());
    }
  }

  EXPECT_EQ(objectIds.size(), 2U);
  EXPECT_TRUE(busNames.count("first_bus") != 0U);
  EXPECT_TRUE(busNames.count("second_bus") != 0U);
}

/// @test
/// Verifies that the merger rejects repeated object names on the same bus.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeFailsForDuplicateObjectNamesOnSameBus)
{
  TempDir tempDir;
  SingleClassSetup setup;

  writeIndexedCreationRecording(
    "first", tempDir, makeObjectInfo(setup.object, "shared_session", "shared_bus"), makeTime(10));
  writeIndexedCreationRecording(
    "second", tempDir, makeObjectInfo(setup.object, "shared_session", "shared_bus"), makeTime(20));

  const auto mergedArchive = makeArchivePath("merged_with_duplicate_object_name", tempDir);
  auto settings =
    makeMergeSettings({makeArchivePath("first", tempDir), makeArchivePath("second", tempDir)}, mergedArchive);

  EXPECT_ANY_THROW(mergeRecordings(settings));
}

/// @test
/// Verifies that equal object names can still be merged when they belong to different buses.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeAllowsEqualObjectNamesOnDifferentBuses)
{
  TempDir tempDir;
  SingleClassSetup setup;

  writeIndexedCreationRecording(
    "first", tempDir, makeObjectInfo(setup.object, "shared_session", "first_bus"), makeTime(10));
  writeIndexedCreationRecording(
    "second", tempDir, makeObjectInfo(setup.object, "shared_session", "second_bus"), makeTime(20));

  const auto mergedArchive = makeArchivePath("merged_with_equal_object_names_on_different_buses", tempDir);
  auto settings =
    makeMergeSettings({makeArchivePath("first", tempDir), makeArchivePath("second", tempDir)}, mergedArchive);

  EXPECT_NO_THROW(mergeRecordings(settings));
}

/// @test
/// Verifies that annotations are merged chronologically and included in the output summary.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, RawMergeOrdersAnnotationsChronologically)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  writeAnnotationRecording("first", tempDir, {{makeTime(30), 30U}, {makeTime(10), 10U}});
  writeAnnotationRecording("second", tempDir, {{makeTime(20), 20U}, {makeTime(40), 40U}});

  const auto mergedArchive = makeArchivePath("merged_annotations", tempDir);
  auto settings =
    makeMergeSettings({makeArchivePath("first", tempDir), makeArchivePath("second", tempDir)}, mergedArchive);
  mergeRecordings(settings);

  Input input(mergedArchive, kernel.getTypes());
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.firstTime, makeTime(10));
  EXPECT_EQ(summary.lastTime, makeTime(40));
  EXPECT_EQ(summary.keyframeCount, 0U);
  EXPECT_EQ(summary.objectCount, 0U);
  EXPECT_EQ(summary.typeCount, 0U);
  EXPECT_EQ(summary.annotationCount, 4U);
  EXPECT_EQ(summary.indexedObjectCount, 0U);

  std::vector<TimeStamp> annotationTimes;
  std::vector<uint8_t> annotationValues;
  auto cursor = input.annotationsBegin();
  while (!cursor.atEnd())
  {
    ++cursor;
    if (!cursor.atEnd())
    {
      const auto& annotation = std::get<Annotation>(cursor.get().payload);
      const auto value = annotation.getValueAsBuffer();

      ASSERT_FALSE(value.empty());
      annotationTimes.push_back(cursor.get().time);
      annotationValues.push_back(value[0]);
    }
  }

  ASSERT_EQ(annotationTimes.size(), 4U);
  EXPECT_EQ(annotationTimes[0], makeTime(10));
  EXPECT_EQ(annotationTimes[1], makeTime(20));
  EXPECT_EQ(annotationTimes[2], makeTime(30));
  EXPECT_EQ(annotationTimes[3], makeTime(40));

  ASSERT_EQ(annotationValues.size(), 4U);
  EXPECT_EQ(annotationValues[0], 10U);
  EXPECT_EQ(annotationValues[1], 20U);
  EXPECT_EQ(annotationValues[2], 30U);
  EXPECT_EQ(annotationValues[3], 40U);
}

/// @test
/// Verifies that aligned merge modes also apply their time shift to annotations.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, MergeAppliesAnnotationOffsets)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  writeAnnotationRecording("first", tempDir, {{makeTime(30), 30U}, {makeTime(40), 40U}});
  writeAnnotationRecording("second", tempDir, {{makeTime(100), 100U}, {makeTime(110), 110U}});

  RecordingMergeSettings settings;
  settings.mode = RecordingMergeMode::offsetAligned;
  settings.outputArchive = makeArchivePath("merged_annotation_offsets", tempDir);
  settings.inputArchives = {makeMergeInput(makeArchivePath("first", tempDir), Duration(std::chrono::seconds(5))),
                            makeMergeInput(makeArchivePath("second", tempDir), Duration(std::chrono::seconds(25)))};
  mergeRecordings(settings);

  Input input(settings.outputArchive, kernel.getTypes());
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.firstTime, makeTime(5));
  EXPECT_EQ(summary.lastTime, makeTime(35));
  EXPECT_EQ(summary.annotationCount, 4U);

  std::vector<TimeStamp> annotationTimes;
  std::vector<uint8_t> annotationValues;
  auto cursor = input.annotationsBegin();
  while (!cursor.atEnd())
  {
    ++cursor;
    if (!cursor.atEnd())
    {
      const auto& annotation = std::get<Annotation>(cursor.get().payload);
      const auto value = annotation.getValueAsBuffer();

      ASSERT_FALSE(value.empty());
      annotationTimes.push_back(cursor.get().time);
      annotationValues.push_back(value[0]);
    }
  }

  ASSERT_EQ(annotationTimes.size(), 4U);
  EXPECT_EQ(annotationTimes[0], makeTime(5));
  EXPECT_EQ(annotationTimes[1], makeTime(15));
  EXPECT_EQ(annotationTimes[2], makeTime(25));
  EXPECT_EQ(annotationTimes[3], makeTime(35));

  ASSERT_EQ(annotationValues.size(), 4U);
  EXPECT_EQ(annotationValues[0], 30U);
  EXPECT_EQ(annotationValues[1], 40U);
  EXPECT_EQ(annotationValues[2], 100U);
  EXPECT_EQ(annotationValues[3], 110U);
}

/// @test
/// Verifies that a recording object formats its fully qualified name.
/// @requirements(SEN-364)
TEST(RecordingMergerTest, RecordingFormatsFullName)
{
  RecordingMergeObject object("", 0U, "session", "bus", "object", "");

  EXPECT_EQ(object.formatFullName(), "session.bus.object");
}

}  // namespace sen::db::test
