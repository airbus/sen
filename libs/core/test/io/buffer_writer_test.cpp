// === buffer_writer_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "stl/sen/kernel/basic_types.stl.h"

// gtest
#include <gtest/gtest.h>

// std
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <tuple>
#include <vector>

template <typename T>
class ResizableBufferWriterTypedTest: public testing::Test
{
protected:
  T buffer;  // NOLINT(misc-non-private-member-variables-in-classes)
  T& getBuffer() { return buffer; }
};

template <>
class ResizableBufferWriterTypedTest<sen::FixedMemoryBlock>: public testing::Test
{
protected:
  void SetUp() override
  {
    pool = sen::FixedMemoryBlockPool<1024>::make();
    blockPtr = pool->getBlockPtr();
  }

  std::shared_ptr<sen::FixedMemoryBlockPool<1024>> pool;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<sen::FixedMemoryBlock> blockPtr;        // NOLINT(misc-non-private-member-variables-in-classes)

  [[nodiscard]] sen::FixedMemoryBlock& getBuffer() const { return *blockPtr; }
};

using BufferTypes =
  testing::Types<std::vector<uint8_t>, sen::ResizableHeapBlock, sen::FixedMemoryBlock, sen::kernel::Buffer>;

TYPED_TEST_SUITE(ResizableBufferWriterTypedTest, BufferTypes);

/// @test
/// Verifies that BufferWriter advance correctly increments the cursor and returns the previous position
/// @requirements(SEN-1051)
TEST(BufferWriterTest, AdvanceIncrementsCursor)
{
  std::array<uint8_t, 10> buffer {};
  const sen::Span<uint8_t> span {buffer};
  sen::BufferWriter writer {span};

  uint8_t* ptr1 = writer.advance(4);
  uint8_t* ptr2 = writer.advance(2);

  EXPECT_EQ(ptr1, buffer.data());
  EXPECT_EQ(ptr2, std::next(buffer.data(), 4));
}

/// @test
/// Verifies that BufferWriter reverse correctly decrements the cursor backwards
/// @requirements(SEN-1051)
TEST(BufferWriterTest, ReverseDecrementsCursor)
{
  std::array<uint8_t, 10> buffer {};
  const sen::Span<uint8_t> span {buffer};
  sen::BufferWriter writer {span};

  std::ignore = writer.advance(5);
  writer.reverse(2);

  uint8_t* currentCursor = writer.advance(0);
  EXPECT_EQ(currentCursor, std::next(buffer.data(), 3));
}

/// @test
/// Verifies that ResizableBufferWriter advance resizes the container and returns a pointer to the newly allocated space
/// @requirements(SEN-1051)
TYPED_TEST(ResizableBufferWriterTypedTest, AdvanceResizesContainer)
{
  auto& buffer = this->getBuffer();
  sen::ResizableBufferWriter<TypeParam> writer {buffer};

  uint8_t* ptr1 = writer.advance(4);
  EXPECT_EQ(buffer.size(), 4);
  EXPECT_EQ(ptr1, buffer.data());

  uint8_t* ptr2 = writer.advance(2);
  EXPECT_EQ(buffer.size(), 6);
  EXPECT_EQ(ptr2, std::next(buffer.data(), 4));
}

/// @test
/// Verifies that ResizableBufferWriter reverse correctly shrinks the container size
/// @requirements(SEN-1051)
TYPED_TEST(ResizableBufferWriterTypedTest, ReverseShrinksContainer)
{
  auto& buffer = this->getBuffer();
  sen::ResizableBufferWriter<TypeParam> writer {buffer};

  std::ignore = writer.advance(5);
  EXPECT_EQ(buffer.size(), 5);

  writer.reverse(2);
  EXPECT_EQ(buffer.size(), 3);
}

/// @test
/// Verifies that ResizableBufferWriter triggers an assertion when reversing more than the current size to prevent
/// size_t underflow
/// @requirements(SEN-1051)
TYPED_TEST(ResizableBufferWriterTypedTest, ReverseUnderflowTriggersAssert)
{
  auto& buffer = this->getBuffer();
  sen::ResizableBufferWriter<TypeParam> writer {buffer};

  std::ignore = writer.advance(5);

  EXPECT_DEATH(writer.reverse(6), "");
}
