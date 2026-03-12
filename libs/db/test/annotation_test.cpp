// === annotation_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/core/meta/native_types.h"
#include "sen/db/annotation.h"
#include "sen/db/input.h"
#include "sen/db/output.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <utility>
#include <vector>

namespace sen::db::test
{

/// @test
/// Write a single annotation and read it back via the annotation cursor
/// @requirements(SEN-364)
TEST(AnnotationTest, WriteAndReadSingleAnnotation)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto typeInt32 = sen::Int32Type::get();

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;
  const auto archivePath = tempDir.path() / settings.name;

  // limited scope to force to close output
  {
    Output output(std::move(settings), []() {});

    // write a keyframe to make it a valid archive
    output.keyframe(kernel.getTime(), {});

    // write one annotation with a simple int32 value
    ::sen::kernel::Buffer annBuf;
    annBuf.push_back(99);
    annBuf.push_back(0);
    annBuf.push_back(0);
    annBuf.push_back(0);
    output.annotation(kernel.getTime(), typeInt32.type(), std::move(annBuf));
  }

  // read the archive
  Input input(archivePath.string(), kernel.getTypes());
  auto cursor = input.annotationsBegin();

  ASSERT_FALSE(cursor.atEnd());

  // advance to the first annotation
  ++cursor;
  ASSERT_FALSE(cursor.atEnd());

  const auto& entry = cursor.get();
  const auto& annotation = std::get<Annotation>(entry.payload);

  // verify the type is the expected one
  EXPECT_EQ(annotation.getType().type()->getName(), typeInt32.type()->getName());

  // verify the buffer content
  auto buffer = annotation.getValueAsBuffer();
  ASSERT_GE(buffer.size(), 4U);
  EXPECT_EQ(buffer[0], 99U);

  // verify getValueAsVariant
  const auto& variant = annotation.getValueAsVariant();
  EXPECT_EQ(variant.getCopyAs<int32_t>(), 99U);

  // advance to check there are no more annotations
  ++cursor;
  EXPECT_TRUE(cursor.atEnd());
}

/// @test
/// Write multiple annotations and verify the count
/// @requirements(SEN-364)
TEST(AnnotationTest, WriteAndReadMultipleAnnotations)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto typeInt32 = sen::Int32Type::get();

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;
  const auto archivePath = tempDir.path() / settings.name;

  constexpr int annotationCount = 5;

  // limited scope to force to close output
  {
    Output output(std::move(settings), []() {});

    // write a keyframe to make it a valid archive
    output.keyframe(kernel.getTime(), {});

    for (int i = 0; i < annotationCount; ++i)
    {
      ::sen::kernel::Buffer annBuf;
      auto val = static_cast<uint8_t>(i);
      annBuf.push_back(val);
      annBuf.push_back(0);
      annBuf.push_back(0);
      annBuf.push_back(0);
      annBuf.insert(annBuf.end(), {val, 0, 0, 0});
      output.annotation(kernel.getTime(), typeInt32.type(), std::move(annBuf));
    }
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto cursor = input.annotationsBegin();

  int count = 0;
  while (!cursor.atEnd())
  {
    ++cursor;
    if (!cursor.atEnd())
    {
      ++count;
    }
  }

  EXPECT_EQ(count, annotationCount);
}

/// @test
/// Verify an archive with no annotations returns an empty cursor
/// @requirements(SEN-364)
TEST(AnnotationTest, EmptyAnnotationsCursor)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;
  const auto archivePath = tempDir.path() / settings.name;

  // create an archive with no annotations
  {
    Output output(std::move(settings), []() {});
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto cursor = input.annotationsBegin();

  // first advance should reach end
  ++cursor;
  EXPECT_TRUE(cursor.atEnd());
}

/// @test
/// Add an annotation using addAnnotation func to an existing archive
/// @requirements(SEN-364)
TEST(AnnotationTest, AddAnnotationPostRecording)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");
  const auto typeInt32 = sen::Int32Type::get();

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;
  const auto archivePath = tempDir.path() / settings.name;

  // create an archive with no annotations
  {
    Output output(std::move(settings), []() {});
    output.keyframe(kernel.getTime(), {});
  }

  // add a post-recording annotation
  {
    Input input(archivePath.string(), kernel.getTypes());
    std::vector<uint8_t> annValue = {99, 0, 0, 0};
    input.addAnnotation(kernel.getTime(), typeInt32.type(), std::move(annValue));
  }

  // read the annotation
  {
    Input input(archivePath.string(), kernel.getTypes());
    auto cursor = input.annotationsBegin();

    ++cursor;
    ASSERT_FALSE(cursor.atEnd());

    const auto& entry = cursor.get();
    const auto& annotation = std::get<Annotation>(entry.payload);
    auto buffer = annotation.getValueAsBuffer();
    ASSERT_GE(buffer.size(), 4U);
    EXPECT_EQ(buffer[0], 99U);

    ++cursor;
    EXPECT_TRUE(cursor.atEnd());
  }
}

}  // namespace sen::db::test
