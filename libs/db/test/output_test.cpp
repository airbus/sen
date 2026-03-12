// === output_test.cpp =================================================================================================
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
#include "sen/kernel/test_kernel.h"
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <utility>

namespace sen::db::test
{

/// @test
/// Creates archive directory and all required files
/// @requirements(SEN-364)
TEST(OutputTest, CreatesArchiveFiles)
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
  }

  EXPECT_TRUE(std::filesystem::exists(archivePath / "summary"));
  EXPECT_TRUE(std::filesystem::exists(archivePath / "types"));
  EXPECT_TRUE(std::filesystem::exists(archivePath / "runtime"));
  EXPECT_TRUE(std::filesystem::exists(archivePath / "indexes"));
}

/// @test
/// Keyframe increments summary count
/// @requirements(SEN-364)
TEST(OutputTest, KeyframeIncrementsSummary)
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
/// Types file is created with archive
/// @requirements(SEN-364)
TEST(OutputTest, TypesFileIsCreated)
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
  }

  EXPECT_TRUE(std::filesystem::exists(archivePath / "types"));
}

/// @test
/// Write various entry types and fetch stats
/// @requirements(SEN-364)
TEST(OutputTest, WriteAllEntryTypes)
{
  TempDir tempDir;
  SingleClassSetup setup;

  const auto typeInt32 = sen::Int32Type::get();

  // Get property and event IDs from metadata
  const auto classType = setup.object->getClass();
  const auto* propMeta = classType.type()->searchPropertyByName("speed");
  ASSERT_NE(propMeta, nullptr);
  const auto* eventMeta = classType.type()->searchEventByName("valueChanged");
  ASSERT_NE(eventMeta, nullptr);

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};

    // Test creation (indexed)
    output.creation(setup.kernel->getTime(), info, true);
    setup.kernel->step();

    // Test property change with proper serialization
    ::sen::kernel::Buffer propBuf;
    {
      sen::ResizableBufferWriter writer(propBuf);
      sen::OutputStream out(writer);
      sen::SerializationTraits<float64_t>::write(out, 99.1);
    }
    output.propertyChange(setup.kernel->getTime(), setup.object->getId(), propMeta->getId(), std::move(propBuf));

    setup.kernel->step();

    // Test event with proper serialization
    ::sen::kernel::Buffer evBuf;
    {
      sen::ResizableBufferWriter writer(evBuf);
      sen::OutputStream out(writer);
      sen::SerializationTraits<float64_t>::write(out, 99.1);
    }
    output.event(setup.kernel->getTime(), setup.object->getId(), eventMeta->getId(), std::move(evBuf));

    // Test annotation
    ::sen::kernel::Buffer annBuf;
    annBuf.push_back(99);
    annBuf.push_back(0);
    annBuf.push_back(0);
    annBuf.push_back(0);
    output.annotation(setup.kernel->getTime(), typeInt32.type(), std::move(annBuf));

    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  // Verify the records exist
  Input input(archivePath.string(), setup.kernel->getTypes());
  EXPECT_EQ(input.getObjectIndexDefinitions().size(), 1U);

  auto cursor = input.annotationsBegin();
  int annotationCount = 0;
  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }
    ++annotationCount;
  }
  EXPECT_EQ(annotationCount, 1);
}

}  // namespace sen::db::test
