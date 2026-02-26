// === output_queue.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_OUTPUT_QUEUE_H
#define SEN_COMPONENTS_ETHER_SRC_OUTPUT_QUEUE_H

// component
#include "util.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/kernel/tracer.h"

// generated code
#include "stl/configuration.stl.h"

// Moody Camel's concurrent queue
#include "moodycamel/concurrentqueue.h"

// std
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace sen::components::ether
{

/// Wrapper of concurrent queue that allows setting of bounds.
/// This class is thread safe.
template <typename T>
class OutputQueue
{
public:
  SEN_NOCOPY_NOMOVE(OutputQueue)

public:
  explicit OutputQueue(const QueueConfig& config): config_(config), logger_(getLogger()) {}
  ~OutputQueue() = default;

public:
  /// Tries to push an element into the queue.
  /// Drops an element when full depending on the eviction policy.
  void push(T&& elem);

  /// Number of discarded elements.
  [[nodiscard]] std::size_t getDropCount() const noexcept { return dropCount_; }

  /// See ConcurrentQueue::try_dequeue_bulk
  template <typename It>
  [[nodiscard]] std::size_t dequeueBulk(It itemFirst, std::size_t max);

  /// Current size (approx.).
  [[nodiscard]] std::size_t getSize() const noexcept { return queue_.size_approx(); }

  /// Configure the tracing
  void configureTracing(std::string name, sen::kernel::Tracer* tracer);

private:
  void setWarning();
  void clearWarning();
  void plotQueueSize();

private:
  struct TraceInfo
  {
    std::string name;
    sen::kernel::Tracer* tracer;
  };

private:
  std::optional<TraceInfo> traceInfo_;
  moodycamel::ConcurrentQueue<T> queue_;
  std::atomic<std::size_t> dropCount_ = 0;
  QueueConfig config_;
  std::shared_ptr<spdlog::logger> logger_;
  bool warning_ = false;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
inline void OutputQueue<T>::push(T&& elem)
{
  if (config_.maxSize == 0)
  {
    queue_.enqueue(std::move(elem));
    plotQueueSize();
    return;
  }

  if (auto currentSize = queue_.size_approx(); currentSize < config_.maxSize)
  {
    queue_.enqueue(std::move(elem));
    plotQueueSize();

    if (config_.warningLevel != 0 && currentSize >= config_.warningLevel)
    {
      setWarning();
    }

    return;
  }

  // if we reach this point, the queue is full, and we need to drop an element

  if (config_.evictionPolicy == QueueEvictionPolicy::dropOldest)
  {
    T val;
    queue_.try_dequeue(val);
    queue_.enqueue(std::move(elem));
    plotQueueSize();
  }

  ++dropCount_;
}

template <typename T>
inline void OutputQueue<T>::setWarning()
{
  if (!warning_)
  {
    warning_ = true;
    logger_->warn("output queue reached the warning level ({} elements)", config_.warningLevel);
  }
}

template <typename T>
inline void OutputQueue<T>::clearWarning()
{
  if (warning_)
  {
    warning_ = false;
  }
}

template <typename T>
template <typename It>
inline std::size_t OutputQueue<T>::dequeueBulk(It itemFirst, std::size_t max)
{
  auto result = queue_.try_dequeue_bulk(itemFirst, max);
  if (config_.warningLevel != 0 && queue_.size_approx() < config_.warningLevel)
  {
    clearWarning();
  }

  plotQueueSize();

  return result;
}

template <typename T>
inline void OutputQueue<T>::configureTracing(std::string name, sen::kernel::Tracer* tracer)
{
  traceInfo_ = TraceInfo {std::move(name), tracer};
}

template <typename T>
void OutputQueue<T>::plotQueueSize()
{
  if (traceInfo_.has_value())
  {
    auto& traceInfo = traceInfo_.value();
    traceInfo.tracer->plot(traceInfo.name, static_cast<int64_t>(queue_.size_approx()));
  }
}

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_OUTPUT_QUEUE_H
