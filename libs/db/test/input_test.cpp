// === input_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/core/base/numbers.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/native_types.h"
#include "sen/db/input.h"
#include "sen/db/output.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <utility>
#include <vector>

namespace sen::db::test
{

/// Helper function to create a valir archive with one keyframe for corruption tests
static std::filesystem::path createValidArchive(const std::filesystem::path& baseDir, sen::kernel::TestKernel& kernel)
{
  OutSettings settings;
  settings.name = "test";
  settings.folder = baseDir.string();
  settings.indexKeyframes = true;

  const auto archivePath = baseDir / settings.name;

  {
    Output output(std::move(settings), []() {});
    output.keyframe(kernel.getTime(), {});
  }

  return archivePath;
}

/// Helper function to corrupt a file
static void corruptFile(const std::filesystem::path& filePath)
{
  std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
  const char garbage[] = {'\x00',
                          '\x01',
                          '\x02',
                          '\x03',
                          '\x04',
                          '\x05',
                          '\x06',
                          '\x07',
                          '\x08',
                          '\x09',
                          '\x0a',
                          '\x0b',
                          '\x0c',
                          '\x0d',
                          '\x0e',
                          '\x0f'};
  ofs.write(garbage, sizeof(garbage));
}

/// @test
/// Open a recording with one keyframe
/// @requirements(SEN-364)
TEST(InputTest, OpenRecordingWithOneKeyframe)
{
  TempDir tempDir;

  auto object = std::make_shared<TestObjImpl>("variantPropObj", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      source->add(object);
      return sen::kernel::done();
    });
  component.onRun([](auto& api) { return api.execLoop(std::chrono::seconds(1), []() {}); });

  sen::kernel::TestKernel kernel(&component);
  kernel.step();

  // get property metadata for serialization
  const auto classType = object->getClass();
  const auto* propMeta = classType.type()->searchPropertyByName("speed");
  ASSERT_NE(propMeta, nullptr);

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {object.get(), "test_session", "test_bus"};
    output.creation(kernel.getTime(), info, true);

    ::sen::kernel::Buffer propBuf;
    {
      sen::ResizableBufferWriter writer(propBuf);
      sen::OutputStream out(writer);
      sen::SerializationTraits<float64_t>::write(out, 123.456);
    }
    output.propertyChange(kernel.getTime(), object->getId(), propMeta->getId(), std::move(propBuf));
    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());
  const auto& summary = input.getSummary();

  EXPECT_EQ(summary.keyframeCount, 2U);
  EXPECT_EQ(summary.objectCount, 1U);

  {
    auto cursor = input.begin();
    int entryCount = 0;
    while (!cursor.atEnd())
    {
      ++cursor;
      if (cursor.atEnd())
      {
        break;
      }
      ++entryCount;
    }
    EXPECT_GE(entryCount, 3);
  }

  auto keyframes = input.getAllKeyframeIndexes();
  ASSERT_EQ(keyframes.size(), 2U);

  auto exactMatch = input.getKeyframeIndex(keyframes[0].time);
  ASSERT_TRUE(exactMatch.has_value());
  EXPECT_EQ(exactMatch->time, keyframes[0].time);

  auto closestMatch = input.getKeyframeIndex(keyframes[0].time);
  ASSERT_TRUE(closestMatch.has_value());

  {
    auto cursor = input.at(keyframes[1]);
    EXPECT_FALSE(cursor.atEnd());
    ++cursor;
  }

  auto indexedObjects = input.getObjectIndexDefinitions();
  ASSERT_EQ(indexedObjects.size(), 1U);
  {
    auto objCursor = input.makeCursor(indexedObjects[0]);
    int objEntryCount = 0;
    while (!objCursor.atEnd())
    {
      ++objCursor;
      if (objCursor.atEnd())
      {
        break;
      }
      ++objEntryCount;
    }
    EXPECT_GE(objEntryCount, 1);
  }

  {
    auto annCursor = input.annotationsBegin();
    ++annCursor;
    EXPECT_TRUE(annCursor.atEnd());
  }
}

/// @test
/// Open a recording with multiple keyframes and reads the summary
/// @requirements(SEN-364)
TEST(InputTest, OpenRecordingWithKeyframes)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());
  const auto& summary = input.getSummary();

  EXPECT_EQ(3U, summary.keyframeCount);
}

/// @test
/// Iterate through entries in a recording using cursor
/// @requirements(SEN-364)
TEST(InputTest, CanIterateThroughEntries)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());

  // Cursor starts at beginning (monostate), ++cursor reads entries until atEnd()
  auto cursor = input.begin();
  EXPECT_TRUE(cursor.atBegining());
  EXPECT_FALSE(cursor.atEnd());

  // Advance and check we can reach the end
  while (!cursor.atEnd())
  {
    ++cursor;
  }

  EXPECT_TRUE(cursor.atEnd());
}

/// @test
/// Handles truncated runtime gracefully
/// @requirements(SEN-364)
TEST(InputTest, TruncatedRuntimeHandledGracefully)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  const auto runtimePath = archivePath / "runtime";

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < 5; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  const auto originalSize = std::filesystem::file_size(runtimePath);
  if (originalSize <= 50U)
  {
    GTEST_SKIP() << "Recording too small to truncate meaningfully";
  }

  // Truncate to simulate crash
  std::filesystem::resize_file(runtimePath, originalSize - 20);

  // Should not throw when opening or iterating
  EXPECT_NO_THROW({
    Input input(archivePath.string(), kernel.getTypes());
    auto cursor = input.begin();

    while (!cursor.atEnd())
    {
      ++cursor;
    }
  });
}

/// @test
/// Verifies that a truncated runtime file triggers EOF handling
/// @requirements(SEN-364)
TEST(InputTest, TruncatedRuntime_EOFProducesFewerEntries)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  const auto runtimePath = archivePath / "runtime";
  constexpr int writtenKeyframes = 10;

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < writtenKeyframes; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  // truncate: remove last byte to guarantee cutting mid-entry
  const auto originalSize = std::filesystem::file_size(runtimePath);
  std::filesystem::resize_file(runtimePath, originalSize - 1);

  // read and count entries
  Input input(archivePath.string(), kernel.getTypes());
  int readEntries = 0;
  auto cursor = input.begin();

  EXPECT_NO_THROW({
    while (!cursor.atEnd())
    {
      ++cursor;
      if (!cursor.atEnd())
      {
        ++readEntries;
      }
    }
  }) << "Reading a truncated file should handle EOF without throwing";

  // EOF handling: truncation must produce fewer entries than written
  EXPECT_GT(readEntries, 0) << "Should read at least some valid entries";
  EXPECT_EQ(readEntries, writtenKeyframes - 1) << "Truncation should invalidate the last entry (EOF handled)";
}

/// @test
/// Add an annotation to an existing archive and read it back
/// @requirements(SEN-364)
TEST(InputTest, AppendAndReadAnnotations)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto typeInt32 = sen::Int32Type::get();

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    // Need at least one keyframe to make it a valid db
    output.keyframe(kernel.getTime(), {});
  }

  {
    Input input(archivePath.string(), kernel.getTypes());

    // Cursor starts in monostate (atBegining). Advance to first entry and expect End.
    auto cursor = input.annotationsBegin();
    EXPECT_TRUE(cursor.atBegining());
    ++cursor;
    EXPECT_TRUE(cursor.atEnd());

    // Add annotation
    std::vector<uint8_t> annValue = {99, 0, 0, 0};  // simplified int32 value
    input.addAnnotation(kernel.getTime(), typeInt32.type(), std::move(annValue));
  }

  // Create a new input to verify the annotation is there
  {
    Input input(archivePath.string(), kernel.getTypes());
    auto cursor = input.annotationsBegin();

    int count = 0;
    while (!cursor.atEnd())
    {
      ++cursor;
      if (cursor.atEnd())
      {
        break;
      }
      ++count;
    }
    EXPECT_EQ(count, 1);
  }
}

/// @test
/// Verify indexed keys and retrieving particular Object cursors
/// @requirements(SEN-364)
TEST(InputTest, RetrieveIndexedObjectsAndKeyframes)
{
  TempDir tempDir;

  auto object = std::make_shared<TestObjImpl>("indexedObj", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      source->add(object);
      return sen::kernel::done();
    });
  component.onRun([](auto& api) { return api.execLoop(std::chrono::seconds(1), []() {}); });

  sen::kernel::TestKernel kernel(&component);
  kernel.step();

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    // Create an object and index it
    ObjectInfo info = {object.get(), "session1", "bus1"};
    output.creation(kernel.getTime(), info, true /* indexed */);

    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());

  // Verify keyframes
  auto keyframes = input.getAllKeyframeIndexes();
  EXPECT_EQ(keyframes.size(), 2U);

  auto fetchedKeyframe = input.getKeyframeIndex(kernel.getTime());  // will get nearest
  EXPECT_TRUE(fetchedKeyframe.has_value());

  // Iterate from specific keyframe
  auto targetedCursor = input.at(keyframes[keyframes.size() - 1]);
  EXPECT_FALSE(targetedCursor.atEnd());

  // Verify indexed objects
  auto indexedObjects = input.getObjectIndexDefinitions();
  EXPECT_EQ(indexedObjects.size(), 1U);

  // Use makeCursor for the found indexed definition
  auto objectCursor = input.makeCursor(indexedObjects[0]);
  EXPECT_TRUE(objectCursor.atBegining());
  ++objectCursor;
  EXPECT_FALSE(objectCursor.atEnd());
}

/// @test
/// Verify opening an invalid archive path throws exception
/// @requirements(SEN-364)
TEST(InputTest, OpenNonExistentArchive)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  EXPECT_ANY_THROW(Input("/invalid/path", kernel.getTypes()));
}

/// @test
/// Verify opening an empty data file throws exception
/// @requirements(SEN-364)
TEST(InputTest, OpenArchiveWithEmptyFile)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  const auto archivePath = tempDir.path() / "test";
  std::filesystem::create_directories(archivePath);

  {
    auto dataFilePath = archivePath / "runtime";
    std::ofstream ofs(dataFilePath, std::ios::binary);
  }

  EXPECT_ANY_THROW(Input(archivePath.string(), kernel.getTypes()));
}

/// @test
///
/// @requirements(SEN-364)
TEST(InputTest, CorruptRuntimeFile)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto archivePath = createValidArchive(tempDir.path(), kernel);

  corruptFile(archivePath / "runtime");

  EXPECT_ANY_THROW(Input(archivePath.string(), kernel.getTypes()));
}

/// @test
///
/// @requirements(SEN-364)
TEST(InputTest, CorruptSummaryFile)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto archivePath = createValidArchive(tempDir.path(), kernel);

  corruptFile(archivePath / "summary");

  EXPECT_ANY_THROW(Input(archivePath.string(), kernel.getTypes()));
}

/// @test
///
/// @requirements(SEN-364)
TEST(InputTest, CorruptAnnotationsFile)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto archivePath = createValidArchive(tempDir.path(), kernel);

  corruptFile(archivePath / "annotations");

  EXPECT_ANY_THROW(Input(archivePath.string(), kernel.getTypes()));
}

/// @test
///
/// @requirements(SEN-364)
TEST(InputTest, CorruptTypesFile)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto archivePath = createValidArchive(tempDir.path(), kernel);

  corruptFile(archivePath / "types");

  EXPECT_ANY_THROW(Input(archivePath.string(), kernel.getTypes()));
}

/// @test
///
/// @requirements(SEN-364)
TEST(InputTest, CorruptIndexesFile)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto archivePath = createValidArchive(tempDir.path(), kernel);

  corruptFile(archivePath / "indexes");

  Input input(archivePath.string(), kernel.getTypes());
  EXPECT_ANY_THROW(std::ignore = input.getAllKeyframeIndexes());
}

}  // namespace sen::db::test
