// === memory_block_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

/// @test
/// Check creation of fixed memory block pools of void pointers and the min block size
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolDefault)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  const std::shared_ptr<FixedMemoryPool> blockPool;

  EXPECT_EQ(blockPool->minBlockSize<10>(), 10);
  EXPECT_EQ(blockPool->minBlockSize<3>(), sizeof(void*));
  EXPECT_EQ(blockPool->minBlockSize<7>(), sizeof(void*));
  EXPECT_EQ(blockPool->minBlockSize<8>(), 8);

#ifndef _WIN32
#  ifdef __has_feature
#    if __has_feature(thread_sanitizer) or __has_feature(address_sanitizer)
  return;  // skip last check when running with TSAN
#    endif
#  endif
  EXPECT_EXIT(std::ignore = blockPool->getBlockPtr(), ::testing::KilledBySignal(SIGSEGV), ".*");  // NOLINT
#endif
}

/// @test
/// Check failure when insufficient bucket size in fixed memory pool creation
/// @requirements(SEN-908)
TEST(MemoryBlock, FixedMemoryBlockPoolInsufficientBucketSize)
{
  using FixedMemoryPool = sen::FixedMemoryBlockPool<sizeof(void*)>;
  EXPECT_DEATH(std::ignore = FixedMemoryPool::make(1, 0), ".*");  // NOLINT
}

/// @test
/// Check preallocation in fixed memory block pools and post-resize operations
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
/// Check reserve operations and exceptions in fixed memory pools
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
/// Check resize in fixed memory block pools without a previous memory preallocation
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
/// Check bucket capacity expansion inside fixed memory block pools
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
/// Check Span method of fixed memory blocks
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

  for (auto i = 0; i < fixedBlock->size(); i++)
  {
    fixedBlock->data()[i] = i;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
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
/// Check move assignment and move construction in fixed memory blocks
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
/// Check Resizable heap block (creation, and basic operations)
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

  for (auto i = 0; i < heap.size(); i++)
  {
    heap.data()[i] = i;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  EXPECT_EQ(heap.getSpan().data(), heap.data());
  EXPECT_EQ(heap.getConstSpan().data(), heap.data());
}

/// @test
/// Check Resizable heap buffer (creation and basic operations)
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
/// Check Sequential resizable heap operations in buffer
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
/// Check fixed buffer list with sequential operations
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
