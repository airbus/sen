// === memory_block_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

/// @test
/// Reports minBlockSize as the requested size clamped up to sizeof(void*), sizes 3, 7, and 8 all yielding 8
/// while 10 stays 10, and a default-made pool hands out a non-null block.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolDefault)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const auto blockPool = FixedMemoryPool::make();

  EXPECT_EQ(blockPool->minBlockSize<10>(), 10);
  EXPECT_EQ(blockPool->minBlockSize<3>(), sizeof(void*));
  EXPECT_EQ(blockPool->minBlockSize<7>(), sizeof(void*));
  EXPECT_EQ(blockPool->minBlockSize<8>(), 8);
  EXPECT_NE(blockPool->getBlockPtr(), nullptr);
}

/// @test
/// Dies on construction when asked for fewer than two blocks per bucket, FixedMemoryBlockPool::make(1, 0)
/// aborting on its assertion.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolInsufficientBucketSize)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  EXPECT_DEATH(std::ignore = FixedMemoryPool::make(1, 0), ".*");  // NOLINT
}

/// @test
/// Hands out a preallocated block that starts empty, grows it with resize within the pool's fixed block size,
/// and throws from resize once the requested size exceeds that block size.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolPreAlloc)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const auto blockPool = FixedMemoryPool::make(30, 20);

  // Ensure pre-allocated blocks exist but are empty
  const auto fixedBlock = blockPool->getBlockPtr();
  EXPECT_NE(fixedBlock, nullptr);
  EXPECT_TRUE(fixedBlock->empty());
  EXPECT_EQ(fixedBlock->size(), 0);

  // resize and check new size and that is not empty
  fixedBlock->resize(5);
  EXPECT_EQ(fixedBlock->size(), 5);
  EXPECT_FALSE(fixedBlock->empty());

  // check data assignment
  fixedBlock->data()[0] = 15;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(fixedBlock->size(), 5);

  // ensure resize fails if new size is greater than block size
  constexpr auto badSize = sizeof(void*) + 1;
  EXPECT_ANY_THROW(fixedBlock->resize(badSize));
  EXPECT_NO_THROW(fixedBlock->resize(badSize - 1));
}

/// @test
/// Accepts reserve up to the pool's fixed block size and throws when the reservation would exceed it.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolReserve)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const auto blockPool = FixedMemoryPool::make(10, 5);
  const auto fixedBlock = blockPool->getBlockPtr();

  EXPECT_NO_THROW(fixedBlock->reserve(sizeof(void*)));

  constexpr auto badSize = sizeof(void*) + 1;
  EXPECT_ANY_THROW(fixedBlock->reserve(badSize));
}

/// @test
/// Resizes a block from a pool made with no bucket preallocation, growing to 7 bytes, shrinking back to empty
/// at size 0, consecutive data() calls returning the same pointer, and throwing when the requested size
/// exceeds the fixed block size.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolNoPreAlloc)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const auto blockPool = FixedMemoryPool::make(2, 0);
  const auto fixedBlock = blockPool->getBlockPtr();

  fixedBlock->resize(7);
  EXPECT_EQ(fixedBlock->size(), 7);
  EXPECT_FALSE(fixedBlock->empty());

  fixedBlock->resize(0);
  EXPECT_EQ(fixedBlock->size(), 0);
  EXPECT_TRUE(fixedBlock->empty());

  // data assignment
  const auto constDataPtr = fixedBlock->data();
  const auto dataPtr = fixedBlock->data();
  EXPECT_EQ(constDataPtr, dataPtr);

  // ensure resize fails if new size greater than block size
  EXPECT_ANY_THROW(fixedBlock->resize(300));
}

/// @test
/// Allocates a third block from a pool bucketed two blocks at a time, growing a new bucket rather than
/// returning null.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolBucketExpansion)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const auto blockPool = FixedMemoryPool::make(2, 1);

  const auto block1 = blockPool->getBlockPtr();
  const auto block2 = blockPool->getBlockPtr();
  const auto block3 = blockPool->getBlockPtr();

  EXPECT_NE(block1, nullptr);
  EXPECT_NE(block2, nullptr);
  EXPECT_NE(block3, nullptr);
}

/// @test
/// Exposes the block through spans, a fresh preallocated block spanning size 0 with non-null data, a resized
/// block spanning its 6 bytes, and getSpan, getConstSpan, and the implicit Span conversions all sharing the
/// block's data pointer.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockSpan)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const auto blockPool = FixedMemoryPool::make(4, 4);
  const auto fixedBlock = blockPool->getBlockPtr();

  // as pre-allocated, size must be 0 but data not empty
  auto span = fixedBlock->getSpan();
  EXPECT_EQ(span.size(), 0);
  EXPECT_NE(span.data(), nullptr);

  fixedBlock->resize(6);
  span = fixedBlock->getSpan();
  EXPECT_EQ(span.size(), 6);
  EXPECT_NE(span.data(), nullptr);

  for (std::size_t i = 0; i < fixedBlock->size(); i++)
  {
    fixedBlock->data()[i] =  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sen::std_util::ignoredLossyConversion<uint8_t>(i);
  }

  EXPECT_EQ(fixedBlock->getSpan().data(), fixedBlock->data());
  EXPECT_EQ(fixedBlock->getConstSpan().data(), fixedBlock->data());

  const sen::Span<uint8_t> implicitSpan = *fixedBlock;
  EXPECT_EQ(implicitSpan.size(), 6);
  EXPECT_EQ(implicitSpan.data(), fixedBlock->data());

  const sen::MemoryBlock& constBlock = *fixedBlock;
  const sen::Span<const uint8_t> implicitConstSpan = constBlock;
  EXPECT_EQ(implicitConstSpan.size(), 6);
  EXPECT_EQ(implicitConstSpan.data(), fixedBlock->data());
}

/// @test
/// Transfers block storage on move, move assignment handing the source's data pointer to the target while the
/// source ends at size 0 holding the target's former storage, and move construction leaving the source at
/// size 0 with null data.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockMoveSemantics)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const auto blockPool = FixedMemoryPool::make(4, 4);
  const auto fixedBlock1 = blockPool->getBlockPtr();
  const auto fixedBlock2 = blockPool->getBlockPtr();

  fixedBlock1->resize(4);

  auto* originalData1 = fixedBlock1->data();
  auto* originalData2 = fixedBlock2->data();
  *fixedBlock2 = std::move(*fixedBlock1);

  EXPECT_EQ(fixedBlock2->size(), 4);
  EXPECT_EQ(fixedBlock2->data(), originalData1);

  EXPECT_EQ(fixedBlock1->size(), 0);
  EXPECT_EQ(fixedBlock1->data(), originalData2);

  sen::FixedMemoryBlock fixedBlock3(std::move(*fixedBlock2));

  EXPECT_EQ(fixedBlock3.size(), 4);
  EXPECT_EQ(fixedBlock3.data(), originalData1);

  EXPECT_EQ(fixedBlock2->size(), 0);
  EXPECT_EQ(fixedBlock2->data(), nullptr);
}

/// @test
/// Starts empty with null data, keeps size 0 while reserve(8) acquires storage, reports size 8 after
/// resize(8), and serves the same data pointer through getSpan and getConstSpan.
/// @requirements(SEN-908)
TEST(MemoryBlock, ResizableHeapBlock)
{
  sen::ResizableHeapBlock heap;

  EXPECT_EQ(heap.size(), 0);
  EXPECT_EQ(heap.data(), nullptr);

  heap.reserve(8);
  EXPECT_EQ(heap.size(), 0);
  EXPECT_NE(heap.data(), nullptr);

  heap.resize(8);
  EXPECT_EQ(heap.size(), 8);

  for (std::size_t i = 0; i < heap.size(); i++)
  {
    heap.data()[i] =  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      sen::std_util::ignoredLossyConversion<uint8_t>(i);
  }

  EXPECT_EQ(heap.getSpan().data(), heap.data());
  EXPECT_EQ(heap.getConstSpan().data(), heap.data());
}

/// @test
/// Grows a ResizableHeapBlock through an OutputStream backed by a ResizableBufferWriter, 6000 single-byte
/// writes leaving the block at exactly 6000 bytes.
/// @requirements(SEN-908)
TEST(MemoryBlock, ResizableHeapBuffer)
{
  const auto buffer = std::make_shared<sen::ResizableHeapBlock>();
  static constexpr uint16_t dataSize = 6000;
  std::vector<uint8_t> dataVec;

  // fill input vector with dummy data
  dataVec.resize(dataSize);
  std::fill(dataVec.begin(), dataVec.end(), 10);

  // reserve space
  buffer->reserve(dataSize);
  sen::ResizableBufferWriter writer(*buffer);
  sen::OutputStream out(writer);

  // write vector data in output stream
  for (const auto data: dataVec)
  {
    out.writeUInt8(data);
  }

  EXPECT_EQ(buffer->size(), dataSize);
}

/// @test
/// Preserves existing bytes while growing one byte at a time, two ResizableHeapBlocks filled to 6000 bytes by
/// repeated resize comparing equal span to span.
/// @requirements(SEN-908)
TEST(MemoryBlock, SequentialResizableHeap)
{
  static constexpr uint16_t numOperations = 6000;

  auto inputBuffer = sen::ResizableHeapBlock();
  auto outputBuffer = sen::ResizableHeapBlock();

  // fill input buffer blocks with dummy data
  for (auto i = 0; i < numOperations; i++)
  {
    // assign dummy data to input buffer
    inputBuffer.resize(i + 1);
    inputBuffer.data()[i] = i % 255;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  // read data from input buffer
  for (auto i = 0; i < numOperations; i++)
  {
    outputBuffer.resize(i + 1);
    outputBuffer.data()[i] = inputBuffer.data()[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  EXPECT_EQ(outputBuffer.getSpan(), inputBuffer.getSpan());
}

/// @test
/// Chains fixed blocks from a 1024 byte pool into a buffer list, appending 256 bytes per step and taking a new
/// block when the current one is full, 5000 appends consuming exactly 1250 blocks.
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedBufferList)
{
  static constexpr uint16_t maxDataSize = 1024;
  static constexpr uint16_t dataSize = 256;
  static constexpr uint16_t numOperations = 5000;

  using FixedBufferList = std::vector<std::shared_ptr<sen::FixedMemoryBlock>>;
  using PoolBlocks = sen::FixedMemoryBlockPool<maxDataSize>;

  const auto pool = PoolBlocks::make();
  FixedBufferList bufferList;
  bufferList.emplace_back(pool->getBlockPtr());

  std::shared_ptr<sen::FixedMemoryBlock> currentBuff = bufferList.back();

  // add a buffer if there is none or if there's not enough space
  for (auto i = 0; i < numOperations; i++)
  {
    // get new block if still no mem blocks or if there's not enough space in current one
    if (currentBuff->size() + dataSize > maxDataSize)
    {
      bufferList.emplace_back(pool->getBlockPtr());
      currentBuff = bufferList.back();
    }

    // set data bytes in current buff
    currentBuff->resize(currentBuff->size() + dataSize);
  }

  EXPECT_EQ(bufferList.size(), numOperations / (maxDataSize / dataSize));
}
