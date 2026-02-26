// === transport.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/transport.h"

// sen
#include "sen/core/base/assert.h"

// moodycamel
#include <concurrentqueue.h>

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace sen::kernel
{

class UniqueByteBufferManagerImpl
{
public:
  using ByteBufferHandle = UniqueByteBufferManager::ByteBufferHandle;
  using BufferType = ByteBufferHandle::element_type;

  /// Returns a handle to a buffer that fix size elements.
  ///
  /// @param size[in]: of the buffer
  ByteBufferHandle getBuffer(size_t size)
  {
    if (std::unique_ptr<BufferType> reusedBuffer; buffers_.try_dequeue(reusedBuffer))
    {
      reusedBuffer->resize(size);
      return {reusedBuffer.release(),
              [this](BufferType* underlyingBuffer) -> void
              { this->transferOwnershipOfUnderlyingBuffer(underlyingBuffer); }};
    }

    return {new BufferType(size),
            [this](BufferType* underlyingBuffer) -> void
            { this->transferOwnershipOfUnderlyingBuffer(underlyingBuffer); }};
  }

private:
  /// Callback to return the ownership of the underlying buffer to the manager for reuse.
  ///
  /// @param: underlyingBuffer[in]: that is no longer needed
  void transferOwnershipOfUnderlyingBuffer(BufferType* underlyingBuffer)
  {
    buffers_.enqueue(std::unique_ptr<BufferType>(underlyingBuffer));
  }

private:
  moodycamel::ConcurrentQueue<std::unique_ptr<BufferType>> buffers_;
};

UniqueByteBufferManager::UniqueByteBufferManager(): pImpl_(std::make_unique<UniqueByteBufferManagerImpl>()) {};
UniqueByteBufferManager::~UniqueByteBufferManager() = default;

UniqueByteBufferManager::ByteBufferHandle UniqueByteBufferManager::getBuffer(size_t size)
{
  return pImpl_->getBuffer(size);
}

uint32_t getKernelProtocolVersion() noexcept { return 9U; }

Transport::Transport(std::string_view sessionName): name_(sessionName)
{
  if (name_.empty())
  {
    std::string err;
    err.append("invalid empty session name");
    throwRuntimeError(err);
  }
}

bool operator==(const ParticipantAddr& lhs, const ParticipantAddr& rhs) noexcept
{
  return lhs.id == rhs.id && lhs.proc == rhs.proc;
}

bool operator!=(const ParticipantAddr& lhs, const ParticipantAddr& rhs) noexcept { return !(lhs == rhs); }

}  // namespace sen::kernel
