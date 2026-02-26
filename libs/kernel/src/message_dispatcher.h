// === message_dispatcher.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_MESSAGE_DISPATCHER_H
#define SEN_LIBS_KERNEL_SRC_MESSAGE_DISPATCHER_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/source_location.h"
#include "sen/kernel/tracer.h"

// moody
#include "blockingconcurrentqueue.h"
#include "concurrentqueue.h"
#include "sen/kernel/transport.h"

// std
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

class MessageDispatcher
{
  using TracerFactoryTy = sen::std_util::move_only_function<std::unique_ptr<Tracer>(std::string_view)>;

  static constexpr size_t bulkProcessingSize {4};
  static constexpr std::chrono::milliseconds dequeueTimeout {5};
  static constexpr size_t queuePressureCutoff {8000 * 8000};  // allows roughly 8000 messages to be in queue
  static constexpr size_t maxThreads {8};
  static constexpr size_t defaultThreads {2};

public:
  UniqueByteBufferManager& getByteBufferManager() { return bufferManager_; }

  class WorkItem
  {
  public:
    using WorkWrapperType = sen::std_util::move_only_function<void()>;

    /// Creates empty work item
    WorkItem() = default;

    /// Creates work item for the given work.
    ///
    /// @param work[in]: callable work that should be done
    /// @param ensureNotDropped[in]: whether the work can be dropped under pressure or needs to be executed
    explicit WorkItem(WorkWrapperType work, bool ensureNotDropped)
      : work_(std::move(work)), canBeDropped_(!ensureNotDropped)
    {
    }

    /// Execute the embedded work
    void doWork() { work_(); }

    /// Whether this work can be dropped or needs to be executed in all cases.
    [[nodiscard]] bool canBeDropped() const { return canBeDropped_; }

  private:
    static constexpr inline void noWork() {}
    WorkWrapperType work_ {&noWork};
    bool canBeDropped_ {true};
  };

  using Work = WorkItem;

private:
  using QueueType = moodycamel::BlockingConcurrentQueue<Work>;

  struct Worker
  {
    explicit Worker(size_t workerId, MessageDispatcher* owner): owner_(owner)
    {
      thread_ = std::thread(
        [this, workerId]()
        {
          std::string threadName {"MessageDispatcherWorker_"};
          threadName += std::to_string(workerId);

          tracer_ = owner_->tracerFactory_(threadName);
          this->processWork();
        });
    }

    /// Runs the processing loop.
    void processWork()
    {
      std::array<Work, bulkProcessingSize> localWorkStorage;

      while (owner_->continueWorking_)
      {
        tracer_->plot("MessageDispatcher::queueSize",
                      std_util::ignoredLossyConversion<int64_t>(owner_->workQueue_.size_approx()));

        size_t numWorkItemsToProcess =
          owner_->workQueue_.wait_dequeue_bulk_timed(localWorkStorage.begin(), bulkProcessingSize, dequeueTimeout);

        if (SEN_LIKELY(numWorkItemsToProcess > 0))
        {
          for (size_t workItemIdx = 0; workItemIdx < numWorkItemsToProcess; ++workItemIdx)
          {
            Work& currentWork = localWorkStorage.at(workItemIdx);
            if (owner_->shouldBeProcessed(currentWork))
            {
              static constexpr auto sl = SEN_SL();
              auto zone = tracer_->makeScopedZone("Process work", sl);

              currentWork.doWork();
            }
            else
            {
              tracer_->message("Dropped work");
            }
          }
        }
      }
    }

    /// Checks whether the worker is joinable.
    [[nodiscard]] bool joinable() const { return thread_.joinable(); }

    /// Waits for the worker to finish execution.
    void join() { thread_.join(); }

  private:
    MessageDispatcher* owner_;
    std::unique_ptr<Tracer> tracer_;
    std::thread thread_;
  };

public:
  explicit MessageDispatcher(sen::std_util::move_only_function<std::unique_ptr<Tracer>(std::string_view)> tracerFactory)
    : tracerFactory_(std::move(tracerFactory))
  {
  }
  SEN_NOCOPY_NOMOVE(MessageDispatcher)
  ~MessageDispatcher()
  {
    continueWorking_ = false;
    for (Worker& worker: workers_)
    {
      if (worker.joinable())
      {
        worker.join();
      }
    }
  }

  /// Start the message dispatcher and the associated worker threads.
  ///
  /// Note: should not be called on a running MessageDispatcher.
  void start()
  {
    SEN_ASSERT(workers_.size() == 0 && "Workers should not be started twice.");

    size_t numWorkers = numRequestedWorkers();

    workers_.reserve(numWorkers);
    for (size_t workerId = 0; workerId < numWorkers; ++workerId)
    {
      workers_.emplace_back(workerId, this);
    }
  }

public:
  /// Enqueue a new work item for concurrent processing.
  void enqueueMessage(Work work) { workQueue_.enqueue(std::move(work)); }

private:
  /// Calculate the number of workers we should spawn.
  [[nodiscard]] size_t numRequestedWorkers() const
  {
    size_t numWorkers {defaultThreads};

    if (const char* envWorkerOverrideVar = std::getenv("SEN_KERNEL_MD_WORKERS"))
    {
      auto envWorkerOverride = sen::std_util::ignoredLossyConversion<size_t>(std::stoi(envWorkerOverrideVar));
      numWorkers = std::max(numWorkers, envWorkerOverride);
    }

    return std::min(numWorkers, maxThreads);
  }

  /// Estimates the pressure that is currently put on the queue through the given scaling function.
  [[nodiscard]] size_t estimateQueuePressure() const
  {
    size_t estimateQueueSize = workQueue_.size_approx();
    return estimateQueueSize * estimateQueueSize;
  }

  /// Determines whether a given work item should be processed.
  [[nodiscard]] bool shouldBeProcessed(const Work& w) const
  {
    if (w.canBeDropped())
    {
      return estimateQueuePressure() < queuePressureCutoff;
    }

    return true;
  }

private:
  QueueType workQueue_;
  std::atomic_bool continueWorking_ {true};
  TracerFactoryTy tracerFactory_;
  std::vector<Worker> workers_;
  UniqueByteBufferManager bufferManager_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_MESSAGE_DISPATCHER_H
