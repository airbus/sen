// === memory_block.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_MEMORY_BLOCK_H
#define SEN_CORE_BASE_MEMORY_BLOCK_H

#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"

#include <memory>
#include <mutex>
#include <utility>

namespace sen
{

/// \addtogroup mem
/// @{

class FixedMemoryBlock;

/// Base class for FixedMemoryBlockPool.
/// Splitting FixedMemoryBlockPool into two classes helps in preventing
/// code bloat, as the subclass is a template that gets instantiated potentially many times.
class FixedMemoryBlockPoolBase
{
  SEN_NOCOPY_NOMOVE(FixedMemoryBlockPoolBase)

public:
  virtual ~FixedMemoryBlockPoolBase() = default;

public:
  /// The minimum block size of a pool
  template <std::size_t size>
  [[nodiscard]] static constexpr std::size_t minBlockSize() noexcept
  {
    return size < sizeof(void*) ? sizeof(void*) : size;
  }

protected:
  /// This class needs the info about the block size, which is passed by the sub-classes.
  explicit FixedMemoryBlockPoolBase(std::size_t blockSize): blockSize_(blockSize) {}
  friend class FixedMemoryBlock;

  /// Returns a memory block. If there are no more blocks
  /// in the pool, a new block will be allocated.
  ///
  /// Throws if maxAlloc blocks are already allocated.
  [[nodiscard]] virtual uint8_t* alloc() = 0;

  /// Releases a memory block and returns it to the pool.
  virtual void free(uint8_t* ptr) noexcept = 0;

  /// The size of the blocks
  [[nodiscard]] std::size_t getBlockSize() const noexcept { return blockSize_; }

private:
  std::size_t blockSize_;
};

/// A simple pool for fixed-size memory blocks.
///
/// The main purpose of this class is to speed-up
/// memory allocations, as well as to reduce memory
/// fragmentation in situations where the same blocks
/// are allocated all over again.
template <std::size_t blockSize>
class FixedMemoryBlockPool final: public FixedMemoryBlockPoolBase,
                                  public std::enable_shared_from_this<FixedMemoryBlockPool<blockSize>>
{
  SEN_NOCOPY_NOMOVE(FixedMemoryBlockPool)
  static_assert(blockSize >= sizeof(void*));

public:  /// Defaults and constants.
  static constexpr std::size_t defaultBlocksPerBucket = 10U;
  static constexpr std::size_t defaultBucketPreAlloc = 10U;

public:
  /// Factory function for instances of this class.
  /// @param blocksPerBucket how many blocks are contained in each bucket.
  /// @param bucketPreAlloc how much space for bucket pointers will be pre-allocated.
  [[nodiscard]] static std::shared_ptr<FixedMemoryBlockPool> make(std::size_t blocksPerBucket = defaultBlocksPerBucket,
                                                                  std::size_t bucketPreAlloc = defaultBucketPreAlloc);

  ~FixedMemoryBlockPool() override
  {
    Lock lock(mutex_);
    buckets_.clear();
  }

public:
  /// Allocate a memory block.
  [[nodiscard]] std::shared_ptr<FixedMemoryBlock> getBlockPtr() noexcept
  {
    return std::make_shared<FixedMemoryBlock>(this->shared_from_this(), alloc());
  }

protected:  // implements FixedMemoryBlockPoolBase
  void free(uint8_t* ptr) noexcept override
  {
    Lock lock(mutex_);
    doFree(ptr);
  }
  [[nodiscard]] uint8_t* alloc() override
  {
    Lock lock(mutex_);
    return doAlloc();
  }

private:
  explicit FixedMemoryBlockPool(std::size_t blocksPerBucket = defaultBlocksPerBucket,
                                std::size_t bucketPreAlloc = defaultBucketPreAlloc);

  void resize();
  void doFree(uint8_t* ptr) noexcept;
  [[nodiscard]] uint8_t* doAlloc();

private:
  class Block;
  friend class FixedMemoryBlock;
  using Mutex = std::recursive_mutex;
  using Lock = std::scoped_lock<Mutex>;

private:
  std::size_t blocksPerBucket_;
  std::vector<std::unique_ptr<std::vector<Block>>> buckets_ = {};
  Block* firstBlock_;
  mutable Mutex mutex_;
};

// Forward declarations
class ResizableHeapBlock;

/// A memory block.
class MemoryBlock
{
public:
  SEN_NOCOPY_NOMOVE(MemoryBlock)

public:
  MemoryBlock() = default;
  virtual ~MemoryBlock() = default;

public:  // basic container interface
  /// Pointer to the memory area (non-const version).
  [[nodiscard]] virtual uint8_t* data() noexcept = 0;

  /// Pointer to the memory area (const version).
  [[nodiscard]] virtual const uint8_t* data() const noexcept = 0;

  /// How many bytes have been used.
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;

  /// True if size == 0.
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  /// Resizes the memory area. Re-allocates memory if needed. Use with care.
  virtual void resize(std::size_t size) = 0;

  /// Set the capacity without increasing the size. Good for preventing re-allocations.
  virtual void reserve(std::size_t size) = 0;

public:  // translation into Span of uint8_t
  [[nodiscard]] Span<const uint8_t> getConstSpan() const noexcept { return makeConstSpan(data(), size()); }
  [[nodiscard]] Span<uint8_t> getSpan() noexcept { return makeSpan(data(), size()); }

public:
  operator Span<const uint8_t>() const noexcept { return getConstSpan(); }  // NOLINT(hicpp-explicit-conversions)
  operator Span<uint8_t>() noexcept { return getSpan(); }                   // NOLINT(hicpp-explicit-conversions)
};

using MemBlockPtr = std::shared_ptr<MemoryBlock>;

/// A pooled block of memory of fixed capacity but variable size.
class FixedMemoryBlock final: public MemoryBlock
{
public:  // special members
  SEN_COPY_CONSTRUCT(FixedMemoryBlock) = delete;
  SEN_COPY_ASSIGN(FixedMemoryBlock) = delete;
  SEN_MOVE_CONSTRUCT(FixedMemoryBlock);
  SEN_MOVE_ASSIGN(FixedMemoryBlock);

public:
  /// This constructor is meant to be used by the pool. Don't directly use it.
  FixedMemoryBlock(std::shared_ptr<FixedMemoryBlockPoolBase> owner, uint8_t* data)
    : owner_(std::move(owner)), data_(data)
  {
  }
  ~FixedMemoryBlock() noexcept override
  {
    if (data_)
    {
      owner_->free(data_);
    }
  }

public:  // basic container interface
  /// Pointer to the memory area (non-const version).
  [[nodiscard]] uint8_t* data() noexcept override { return data_; }

  /// Pointer to the memory area (const version).
  [[nodiscard]] const uint8_t* data() const noexcept override { return data_; }

  /// How many bytes have been used.
  [[nodiscard]] std::size_t size() const noexcept override { return size_; }

  /// Sets the number of used bytes. Does not allocate.
  /// Throws if the size is larger than the block capacity.
  void resize(std::size_t size) override;

  /// Does nothing, but exists to keep the container interface uniform.
  void reserve(std::size_t size) override;

private:
  std::shared_ptr<FixedMemoryBlockPoolBase> owner_ {nullptr};
  uint8_t* data_ {nullptr};
  std::size_t size_ = 0U;
};

/// A resizable memory block that resides on the heap.
class ResizableHeapBlock final: public MemoryBlock
{
public:
  SEN_NOCOPY_NOMOVE(ResizableHeapBlock)

public:
  ResizableHeapBlock() = default;
  ~ResizableHeapBlock() noexcept override = default;

public:  // basic container interface
  /// Pointer to the memory area (non-const version).
  [[nodiscard]] uint8_t* data() noexcept override { return data_.data(); }

  /// Pointer to the memory area (const version).
  [[nodiscard]] const uint8_t* data() const noexcept override { return data_.data(); }

  /// How many bytes have been used.
  [[nodiscard]] std::size_t size() const noexcept override { return data_.size(); }

  /// Resizes the memory area. Re-allocates memory if needed. Use with care.
  void resize(std::size_t size) override { data_.resize(size); }

  /// Set the capacity without increasing the size. Good for preventing re-allocations.
  void reserve(std::size_t size) override { data_.reserve(size); }

private:
  std::vector<uint8_t> data_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// FixedMemoryBlock
//--------------------------------------------------------------------------------------------------------------

inline FixedMemoryBlock::FixedMemoryBlock(FixedMemoryBlock&& other) noexcept
  : owner_(std::exchange(other.owner_, nullptr))
  , data_(std::exchange(other.data_, nullptr))
  , size_(std::exchange(other.size_, 0U))
{
}

//! @cond doxygen_suppress
inline FixedMemoryBlock& FixedMemoryBlock::operator=(FixedMemoryBlock&& other) noexcept
{
  std::swap(owner_, other.owner_);
  std::swap(data_, other.data_);
  std::swap(size_, other.size_);

  return *this;
}

//--------------------------------------------------------------------------------------------------------------
// FixedMemoryBlockPool::Block
//--------------------------------------------------------------------------------------------------------------

template <std::size_t blockSize>
class FixedMemoryBlockPool<blockSize>::Block
{
  SEN_MOVE_ONLY(Block)

public:
  Block() noexcept = default;
  ~Block() = default;

public:
  [[nodiscard]] Block* getNext() noexcept
  {
    return memory_.next;  // NOLINT
  }

  void setNext(Block* next) noexcept
  {
    memory_.next = next;  // NOLINT
  }

private:
  union Memory  // NOLINT
  {
    std::array<uint8_t, blockSize> buffer;
    Block* next = nullptr;
  };

  Memory memory_;
};

//--------------------------------------------------------------------------------------------------------------
// FixedMemoryBlockPool
//--------------------------------------------------------------------------------------------------------------

template <std::size_t blockSize>
inline std::shared_ptr<FixedMemoryBlockPool<blockSize>> FixedMemoryBlockPool<blockSize>::make(
  std::size_t blocksPerBucket,
  std::size_t bucketPreAlloc)
{
  return std::shared_ptr<FixedMemoryBlockPool>(new FixedMemoryBlockPool(blocksPerBucket, bucketPreAlloc));
}

template <std::size_t blockSize>
inline FixedMemoryBlockPool<blockSize>::FixedMemoryBlockPool(std::size_t blocksPerBucket, std::size_t bucketPreAlloc)
  : FixedMemoryBlockPoolBase(blockSize), blocksPerBucket_(blocksPerBucket)
{
  SEN_ASSERT(blocksPerBucket_ >= 2);

  buckets_.reserve(bucketPreAlloc);
  resize();
}

template <std::size_t blockSize>
SEN_ALWAYS_INLINE uint8_t* FixedMemoryBlockPool<blockSize>::doAlloc()
{
  if (firstBlock_ == nullptr)
  {
    resize();
  }

  auto result = firstBlock_;
  firstBlock_ = firstBlock_->getNext();
  return reinterpret_cast<uint8_t*>(result);  // NOLINT
}

template <std::size_t blockSize>
SEN_ALWAYS_INLINE void FixedMemoryBlockPool<blockSize>::doFree(uint8_t* ptr) noexcept  // NOLINT
{
  auto nextFree = firstBlock_;
  firstBlock_ = new (static_cast<void*>(ptr)) Block();  // NOLINT(cppcoreguidelines-owning-memory)
  firstBlock_->setNext(nextFree);
}

template <std::size_t blockSize>
inline void FixedMemoryBlockPool<blockSize>::resize()
{
  // ensure we have reserved space for new buckets
  if (buckets_.size() == buckets_.capacity())
  {
    buckets_.reserve(buckets_.capacity() * 2);
  }

  // create the list of blocks
  auto bucketPtr = std::make_unique<std::vector<Block>>(blocksPerBucket_);

  // set the _next_ pointer on all the blocks of the bucket
  for (std::size_t i = 0; i < bucketPtr->size() - 2U; ++i)
  {
    (*bucketPtr)[i].setNext(&(*bucketPtr)[i + 1U]);  // NOLINT
  }

  // ensure the last one is the final one
  SEN_ENSURE(bucketPtr->back().getNext() == nullptr);  // NOLINT

  // make our first block point to the new bucket
  firstBlock_ = &bucketPtr->front();

  // save the list of blocks
  buckets_.push_back(std::move(bucketPtr));  // NOLINT
}

}  // namespace sen

#endif  // SEN_CORE_BASE_MEMORY_BLOCK_H
