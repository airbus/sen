// === work_queue.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_WORK_QUEUE_H
#define SEN_CORE_OBJ_DETAIL_WORK_QUEUE_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/move_only_function.h"

// std
#include <memory>

namespace sen::impl
{

// Represents a unit of work.
using Call = sen::std_util::move_only_function<void()>;

class WorkQueueImpl;

/// A high-performance queue of tasks.
/// Thread-safe
class WorkQueue final
{
public:
  SEN_NOCOPY_NOMOVE(WorkQueue)

public:
  WorkQueue(std::size_t maxSize, bool dropOldest);
  ~WorkQueue();

public:
  /// Allows pushing into the queue.
  void enable();

  /// Disables pushing into the queue.
  void disable();

  /// Removes any enqueued element.
  void clear();

  /// If enabled, pushes the function into the queue.
  void push(Call&& call, bool force);

  /// Execute all the queued functions. Returns true if at least one function got executed.
  [[nodiscard]] bool executeAll();

  /// Waits on the queue for the arrival of one task, and executes it.
  void waitExecuteOne();

  /// Waits on the queue for the arrival of one or more task, and executes them all.
  void waitExecuteAll(const Duration& timeout);

  /// Sets a function to be called when work is dropped
  void setOnDropped(std_util::move_only_function<void(const Call&)>&& callback);

  /// The estimated current size
  [[nodiscard]] std::size_t getCurrentSize() const noexcept;

private:
  std::unique_ptr<WorkQueueImpl> pimpl_;
};

}  // namespace sen::impl

#endif  // SEN_CORE_OBJ_DETAIL_WORK_QUEUE_H
