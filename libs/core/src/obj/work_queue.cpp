// === work_queue.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/detail/work_queue.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/move_only_function.h"

// Moody Camel's concurrent queue
#include "moodycamel/blockingconcurrentqueue.h"

// std
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace sen::impl
{

//------------------------------------------------------------------------------------------------//
// helpers
//------------------------------------------------------------------------------------------------//

namespace
{

constexpr std::size_t defaultBulkBufferSize = 10U;

}

using BlockingQueue = moodycamel::BlockingConcurrentQueue<Call>;

//------------------------------------------------------------------------------------------------//
// WorkQueueImpl
//------------------------------------------------------------------------------------------------//

class WorkQueueImpl final
{
  SEN_NOCOPY_NOMOVE(WorkQueueImpl)

public:
  WorkQueueImpl(std::size_t maxSize, bool dropOldest): maxSize_(maxSize), dropOld_(dropOldest)
  {
    callBuffer_.reserve(32U * BlockingQueue::BLOCK_SIZE);  // NOLINT
  }

  ~WorkQueueImpl() = default;

public:
  SEN_ALWAYS_INLINE void enable() noexcept  // NOSONAR
  {
    enabled_.store(true);
  }

  SEN_ALWAYS_INLINE void disable() noexcept  // NOSONAR
  {
    enabled_.store(false);
  }

  SEN_ALWAYS_INLINE void push(Call&& call, bool force)  // NOSONAR
  {
    if (!enabled_)
    {
      return;
    }

    if (maxSize_ == 0 || force)
    {
      queue_.enqueue(std::move(call));
      return;
    }

    auto currentSize = queue_.size_approx();
    if (currentSize < maxSize_)
    {
      queue_.enqueue(std::move(call));
      return;
    }

    onDropped_(call);

    if (dropOld_)
    {
      Call discardedCall;
      queue_.try_dequeue(discardedCall);
      queue_.enqueue(std::move(call));
      return;
    }
  }

  void setOnDropped(sen::std_util::move_only_function<void(const Call&)>&& callback)
  {
    onDropped_ = std::move(callback);
  }

  SEN_ALWAYS_INLINE bool executeAll()  // NOSONAR
  {
    callBuffer_.resize(std::max<std::size_t>(queue_.size_approx(), defaultBulkBufferSize));

    if (auto callCount = queue_.try_dequeue_bulk(callBuffer_.begin(), callBuffer_.size()); callCount != 0U)
    {
      std::size_t i = 0;
      for (auto& call: callBuffer_)
      {
        call();
        ++i;

        if (i >= callCount)
        {
          break;
        }
      }
      return true;
    }

    return false;
  }

  SEN_ALWAYS_INLINE void clear()  // NOSONAR
  {
    queue_ = BlockingQueue();
  }

  SEN_ALWAYS_INLINE void waitExecuteOne()  // NOSONAR
  {
    Call call;
    queue_.wait_dequeue(call);
    call();
  }

  SEN_ALWAYS_INLINE void waitExecuteAll(const Duration& timeout)  // NOSONAR
  {
    std::array<Call, defaultBulkBufferSize> calls;
    auto callCount = queue_.wait_dequeue_bulk_timed(calls.begin(), calls.size(), timeout.toChrono());

    if (callCount > 0U)
    {
      std::size_t i = 0;
      for (auto& call: calls)
      {
        call();
        ++i;

        if (i >= callCount)
        {
          break;
        }
      }
    }
  }

  [[nodiscard]] std::size_t getCurrentSize() const noexcept { return queue_.size_approx(); }

private:
  BlockingQueue queue_;
  std::atomic_bool enabled_ {false};
  std::vector<Call> callBuffer_;
  std::size_t maxSize_;
  bool dropOld_ = true;
  sen::std_util::move_only_function<void(const Call&)> onDropped_ = [](const Call& /*call*/) {};
};

//------------------------------------------------------------------------------------------------//
// WorkQueue
//------------------------------------------------------------------------------------------------//

WorkQueue::WorkQueue(std::size_t maxSize, bool dropOldest): pimpl_(std::make_unique<WorkQueueImpl>(maxSize, dropOldest))
{
}

WorkQueue::~WorkQueue() = default;

void WorkQueue::enable() { pimpl_->enable(); }

void WorkQueue::disable() { pimpl_->disable(); }

void WorkQueue::clear() { pimpl_->clear(); }

void WorkQueue::push(Call&& call, bool force) { pimpl_->push(std::move(call), force); }

bool WorkQueue::executeAll() { return pimpl_->executeAll(); }

void WorkQueue::waitExecuteOne() { pimpl_->waitExecuteOne(); }

void WorkQueue::waitExecuteAll(const Duration& timeout) { pimpl_->waitExecuteAll(timeout); }

void WorkQueue::setOnDropped(sen::std_util::move_only_function<void(const Call&)>&& callback)
{
  pimpl_->setOnDropped(std::move(callback));
}

std::size_t WorkQueue::getCurrentSize() const noexcept { return pimpl_->getCurrentSize(); }

}  // namespace sen::impl
