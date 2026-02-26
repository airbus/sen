// === iterator_adapters.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_ITERATOR_ADAPTERS_H
#define SEN_CORE_BASE_ITERATOR_ADAPTERS_H

// std
#include <cstddef>
#include <iterator>

namespace sen::util
{

//===--------------------------------------------------------------------------------------------------------------===//
// Base Definitions
//===--------------------------------------------------------------------------------------------------------------===//

/// Basic range abstraction to form a minimal range around two iterators.
template <typename IteratorType>
struct IteratorRange
{
  IteratorRange(IteratorType begin, IteratorType end): begin_(std::move(begin)), end_(std::move(end)) {}

  [[nodiscard]] IteratorType begin() const { return begin_; }
  [[nodiscard]] IteratorType end() const { return end_; }
  [[nodiscard]] bool empty() const { return begin_ == end_; }

private:
  IteratorType begin_;
  IteratorType end_;
};

/// Create a simple range for the two iterators.
///
/// @param[in] begin: of the range
/// @param[in] end: of the range
///
/// Example:
/// @code{.cpp}
///   struct Foo
///   {
///     auto values() const { return makeRange(myValues.begin(), myValues.end()); }
///   private:
///     std::vector<int> myValues;
///   };
/// @endcode
template <typename IteratorType>
IteratorRange<IteratorType> makeRange(IteratorType begin, IteratorType end)
{
  return {std::move(begin), std::move(end)};
}

/// Create a simple range for the given container.
///
/// @param[in] container: to iterate over
///
/// Example:
/// @code{.cpp}
///   struct Foo
///   {
///     auto values() const { return makeRange(myValues); }
///   private:
///     std::vector<int> myValues;
///   };
/// @endcode
template <typename ContainerType>
auto makeRange(ContainerType& container)
{
  return makeRange(std::begin(container), std::end(container));
}

//===--------------------------------------------------------------------------------------------------------------===//
// Range Adapters
//===--------------------------------------------------------------------------------------------------------------===//

/// A range adapter to combine a range with lock.
///
/// This adapter ties together a lock of the specified LockType with the given range (i.e., the begin/end iterator
/// pair). This way, the adapter ensures that the lock is held during iteration.
///
/// The lock is taken during construction and release during destructions of this adapter.
template <template <typename> typename LockType, typename IteratorType, typename MutexType>
class LockedRangeAdapter
{
public:
  template <typename ContainerType>
  LockedRangeAdapter(ContainerType& container, MutexType& m)
    : lock_(m), beginIterator_(std::begin(container)), endIterator_(std::end(container))
  {
  }

  LockedRangeAdapter(IteratorType begin, IteratorType end, MutexType& m)
    : lock_(m), beginIterator_(begin), endIterator_(end)
  {
  }

  IteratorType begin() const noexcept(std::is_nothrow_copy_constructible_v<IteratorType>) { return beginIterator_; }
  IteratorType end() const noexcept(std::is_nothrow_copy_constructible_v<IteratorType>) { return endIterator_; }

private:
  LockType<MutexType> lock_;
  IteratorType beginIterator_;
  IteratorType endIterator_;
};

/// Create a LockedRangeAdapter for the given iterator pair and lock the given mutex.
///
/// @tparam LockType: type of the lock that should be created
///
/// @param[in] begin: of the range
/// @param[in] end: of the range
/// @param[in] mutex: that should be locked
///
/// Example:
/// @code{.cpp}
///   for (auto x : makeLockedRange<std::lock_guard>(foo.begin(), foo.end(), fooMutex))
///   {
///     // ensures the lock is held during the loop
///   } // lock release after the loop
/// @endcode
template <template <typename> typename LockType, typename IteratorType, typename MutexType>
auto makeLockedRange(IteratorType begin, IteratorType end, MutexType& m)
{
  return LockedRangeAdapter<LockType, IteratorType, MutexType>(begin, end, m);
}

/// Create a LockedRangeAdapter for the given container and lock the given mutex.
///
/// @tparam LockType: type of the lock that should be created
///
/// @param[in] container: to wrap the range around
/// @param[in] mutex: that should be locked
///
/// Example:
/// @code{.cpp}
///   for (auto x : makeLockedRange<std::lock_guard>(fooVector, fooMutex))
///   {
///     // ensures the lock is held during the loop
///   } // lock release after the loop
/// @endcode
template <template <typename> typename LockType, typename ContainerType, typename MutexType>
auto makeLockedRange(ContainerType& container, MutexType& m)
{
  if constexpr (std::is_const_v<ContainerType>)
  {
    return LockedRangeAdapter<LockType, typename ContainerType::const_iterator, MutexType>(container, m);
  }
  else
  {
    return LockedRangeAdapter<LockType, typename ContainerType::iterator, MutexType>(container, m);
  }
}

//===--------------------------------------------------------------------------------------------------------------===//
// Iterator Adapters
//===--------------------------------------------------------------------------------------------------------------===//

/// Iterator adapter to turn a iterator that iterates over a smart ptr sequence into one that iterates over pointers to
/// the underlying type.
///
/// Example:
/// @code{.cpp}
///   struct Foo
///   {
///     auto values() const // user now iterates over `int *`
///     {
///       return makeRange(SmartPtrIteratorAdapter(myValues.begin(), SmartPtrIteratorAdapter(myValues.end()));
///     }
///   private:
///     std::vector<std::unique_ptr<int>> myValues;
///   };
/// @endcode
template <typename IteratorType>
struct SmartPtrIteratorAdapter
{
  explicit SmartPtrIteratorAdapter(IteratorType iter): iter_(iter) {}

  // NOLINTBEGIN(readability-identifier-naming): iterator tags
  using iterator_category = typename IteratorType::iterator_category;
  using difference_type = std::ptrdiff_t;
  using value_type = typename IteratorType::value_type::element_type;
  using pointer = value_type*;
  using reference = value_type&;
  // NOLINTEND(readability-identifier-naming)

  pointer operator*() const { return iter_->get(); }
  reference operator->() const { return operator*(); }

  SmartPtrIteratorAdapter& operator++()
  {
    ++iter_;
    return *this;
  }
  SmartPtrIteratorAdapter operator++(int)
  {
    auto old = *this;
    operator++();
    return old;
  }

  friend bool operator==(const SmartPtrIteratorAdapter& lhs, const SmartPtrIteratorAdapter& rhs)
  {
    return lhs.iter_ == rhs.iter_;
  }
  friend bool operator!=(const SmartPtrIteratorAdapter& lhs, const SmartPtrIteratorAdapter& rhs)
  {
    return !(lhs == rhs);
  }

private:
  IteratorType iter_;
};

}  // namespace sen::util

#endif  // SEN_CORE_BASE_ITERATOR_ADAPTERS_H
