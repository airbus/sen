// === circular_list.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_CIRCULAR_LIST_H
#define SEN_COMPONENTS_EXPLORER_SRC_CIRCULAR_LIST_H

#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

#include <vector>

template <class T>
class CircularList
{
public:
  SEN_COPY_MOVE(CircularList)

public:
  explicit CircularList(std::size_t maxCapacity) noexcept
  {
    SEN_ASSERT((maxCapacity > 0 && ((maxCapacity & (maxCapacity - 1)) == 0)) != 0);

    list_.resize(maxCapacity);
  }
  ~CircularList() = default;

public:
  void clear() noexcept { head_ = tail_ = 0; }
  void push(const T& element) noexcept;
  [[nodiscard]] const T& at(std::size_t idx) const noexcept
  {
    SEN_ASSERT(!empty());
    return list_[(head_ + idx) & (list_.size() - 1)];
  }
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t capacity() const noexcept { return (list_.size() - 1); }
  [[nodiscard]] std::size_t remainingCapacity() const noexcept { return (capacity() - size()); }
  [[nodiscard]] bool empty() const noexcept { return (head_ == tail_); }
  [[nodiscard]] bool isFull() const noexcept { return (remainingCapacity() <= 0); }

private:
  std::size_t head_ = 0U;
  std::size_t tail_ = 0U;
  std::vector<T> list_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <class T>
inline void CircularList<T>::push(const T& element) noexcept
{
  if (isFull())
  {
    head_ = (head_ + 1) & (list_.size() - 1);
  }

  list_[tail_] = element;
  tail_ = (tail_ + 1) & (list_.size() - 1);
}

template <class T>
inline std::size_t CircularList<T>::size() const noexcept
{
  if (empty())
  {
    return 0;
  }

  return list_.size() - ((head_ - tail_) & (list_.size() - 1));
}

#endif  // SEN_COMPONENTS_EXPLORER_SRC_CIRCULAR_LIST_H
