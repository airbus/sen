// === containers.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_CONTAINERS_H
#define SEN_LIBS_KERNEL_SRC_BUS_CONTAINERS_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"

// std
#include <cstdlib>
#include <functional>
#include <list>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

/// Container with fixed time access/insertion/deletion to
/// elements at any point in the sequence.
///
/// It uses a std::list (usually implemented as a double linked list)
/// along with an std::unordered_map (hash map) of iterators to the
/// std::list elements to allow constant time access.
template <typename T>
class List
{
  SEN_MOVE_ONLY(List)

public:
  using Iterator = typename std::list<T>::iterator;
  using ConstIterator = typename std::list<T>::const_iterator;
  using Type = T;
  using SizeType = std::size_t;

public:  // special functions
  List() = default;
  ~List() = default;

public:  // element Access
  /// Returns a read/write reference to the data at the first element of the list.
  [[nodiscard]] T& front() { return list_.front(); }

  /// Returns a read reference to the data at the first element of the list.
  [[nodiscard]] const T& front() const { return list_.front(); }

public:  // iterators
  /// Returns a read/write iterator that points to the first element in the list.
  [[nodiscard]] Iterator begin() { return list_.begin(); }

  /// Returns a read-only (constant) iterator that points to the
  /// first element in the list.
  [[nodiscard]] ConstIterator begin() const { return list_.begin(); }

  /// Returns a read/write iterator that points to the last element in the list.
  [[nodiscard]] Iterator end() { return list_.end(); }

  /// Returns a read-only (constant) iterator that points to the
  /// last element in the list.
  [[nodiscard]] ConstIterator end() const { return list_.end(); }

public:  // capacity
  /// Returns true if the list is empty.
  [[nodiscard]] bool empty() const noexcept { return list_.empty(); }

  /// Returns the number of elements in the list.
  [[nodiscard]] SizeType size() const noexcept { return list_.size(); }

public:  // modifiers
  /// Creates an element at the end of the list and assigns the given data to it.
  /// If the element is already in the list it is not added again.
  void add(T value)
  {
    if (contains(value))
    {
      return;
    }

    list_.push_back(value);
    iteratorsMap_.emplace(value, std::prev(list_.end()));
  }

  /// Removes given element if it is in the list. This operation can be done in constant time.
  void remove(T value)
  {
    if (auto itr = iteratorsMap_.find(value); itr != iteratorsMap_.end())
    {
      list_.erase(itr->second);
      iteratorsMap_.erase(itr);
    }
  }

  /// Erases all the elements.
  void clear()
  {
    list_.clear();
    iteratorsMap_.clear();
  }

public:  // lookup
  /// Checks if the container contains element.
  [[nodiscard]] bool contains(T value) const noexcept { return iteratorsMap_.find(value) != iteratorsMap_.end(); }

private:
  std::list<T> list_;
  std::unordered_map<T, Iterator> iteratorsMap_;
};

/// Bidirectional map container in which both types can be used as key.
///
/// A BiMap<X,Y> is implemented as a combination of a std::unordered_map<X,Y> and
/// a std::unordered_map<Y,X>.
template <typename X, typename Y>
class BiMap
{
  SEN_MOVE_ONLY(BiMap)

public:  // checks
  static_assert(std::is_pointer_v<X> || shouldBePassedByValueV<X>);
  static_assert(std::is_pointer_v<Y> || shouldBePassedByValueV<Y>);

public:
  BiMap() = default;
  ~BiMap() = default;

public:  // capacity
  /// Returns true if the container is empty.
  [[nodiscard]] bool empty() const noexcept { return xToYMap_.empty(); }

  /// Returns the number of elements in the container.
  [[nodiscard]] std::size_t size() const noexcept { return xToYMap_.size(); }

public:  // modifiers
  /// Inserts a pair of elements. If some element is already included in the list
  /// it returns without modification.
  void add(X xElement, Y yElement)
  {
    if (xToYMap_.find(xElement) != xToYMap_.end() || yToXMap_.find(yElement) != yToXMap_.end())
    {
      return;
    }

    xToYMap_[xElement] = yElement;
    yToXMap_[yElement] = xElement;
  }

  /// Inserts a pair of elements in the opposite order. If some element is already
  /// included in the list it returns without modification.
  void add(Y yElement, X xElement) { add(xElement, yElement); }

  /// Removes element pair given X value if it is in the list.
  /// This operation can be done in constant time.
  void remove(X xElement)
  {
    yToXMap_.erase(xToYMap_[xElement]);
    xToYMap_.erase(xElement);
  }

  /// Removes element pair given Y value if it is in the list.
  /// This operation can be done in constant time.
  void remove(Y yElement)
  {
    xToYMap_.erase(yToXMap_[yElement]);
    yToXMap_.erase(yElement);
  }

  /// Erases all the elements.
  void clear()
  {
    xToYMap_.clear();
    yToXMap_.clear();
  }

public:  // lookup
  /// Access specified element. Returns nullptr if not present.
  [[nodiscard]] const Y* tryGet(X xElement) const noexcept
  {
    auto itr = xToYMap_.find(xElement);
    if (itr == xToYMap_.end())
    {
      return nullptr;
    }
    return &(itr->second);
  }

  /// Access specified element. Returns nullptr if not present.
  [[nodiscard]] const X* tryGet(Y yElement) const noexcept
  {
    auto itr = yToXMap_.find(yElement);
    if (itr == yToXMap_.end())
    {
      return nullptr;
    }
    return &(itr->second);
  }

  /// Checks if the container contains X element.
  [[nodiscard]] bool contains(X xElement) const noexcept { return xToYMap_.find(xElement) != xToYMap_.end(); }

  /// Checks if the container contains Y element.
  [[nodiscard]] bool contains(Y yElement) const noexcept { return yToXMap_.find(yElement) != yToXMap_.end(); }

private:
  std::unordered_map<X, Y> xToYMap_;
  std::unordered_map<Y, X> yToXMap_;
};

/// Associative container which maps every element with
/// a list of elements of the other type.
///
/// A BiMultiMap<X,Y> allows to use both types as a key, giving access to the
/// corresponding list of associated elements.
/// It uses std::unordered_map with sen::List in order to allow fixed time
/// pairs insertion/deletion while keeping fast traversal.
template <typename X, typename Y>
class BiMultiMap
{
  SEN_MOVE_ONLY(BiMultiMap)

public:
  using Orphans = std::pair<std::vector<X>&, std::vector<Y>&>;

public:
  BiMultiMap() = default;
  ~BiMultiMap() = default;

public:  // iterators
  /// Returns a read-only (constant) iterator that points to the first element in the xToYListMap.
  template <typename Z, typename = std::enable_if_t<std::is_same_v<Z, X>>>
  [[nodiscard]] typename std::unordered_map<Z, List<Y>>::const_iterator begin() const
  {
    return xToYListMap_.begin();
  }

  /// Returns a read-only (constant) iterator that points to the last element in the xToYListMap.
  template <typename Z, typename = std::enable_if_t<std::is_same_v<Z, X>>>
  [[nodiscard]] typename std::unordered_map<Z, List<Y>>::const_iterator end() const
  {
    return xToYListMap_.end();
  }

  /// Returns a read-only (constant) iterator that points to the first element in the yToXListMap.
  template <typename Z, typename = std::enable_if_t<std::is_same_v<Z, Y>>>
  [[nodiscard]] typename std::unordered_map<Z, List<X>>::const_iterator begin() const
  {
    return yToXListMap_.begin();
  }

  /// Returns a read-only (constant) iterator that points to the last element in the yToXListMap.
  template <typename Z, typename = std::enable_if_t<std::is_same_v<Z, Y>>>
  [[nodiscard]] typename std::unordered_map<Z, List<X>>::const_iterator end() const
  {
    return yToXListMap_.end();
  }

public:  // capacity
  /// Returns true if the container is empty.
  [[nodiscard]] bool empty() const { return xToYListMap_.empty() && yToXListMap_.empty(); }

  /// Returns the number of X or Y elements in the list.
  template <typename Z, typename = std::enable_if_t<std::is_same_v<Z, X> || std::is_same_v<Z, Y>>>
  [[nodiscard]] std::size_t size() const
  {
    if constexpr (std::is_same_v<Z, X>)
    {
      return xToYListMap_.size();
    }
    else
    {
      return yToXListMap_.size();
    }
  }

public:  // modifiers
  /// Inserts a pair of elements.
  /// If the element is already in the list it is not added again.
  void add(X xElement, Y yElement)
  {
    xToYListMap_[xElement].add(yElement);
    yToXListMap_[yElement].add(xElement);
  }

  /// Inserts a pair of elements in the opposite order.
  /// If the element is already in the list it is not added again.
  void add(Y yElement, X xElement) { add(xElement, yElement); }

  /// Removes element pair given X and Y values if they are in the container.
  /// Returns orphan elements (elements that are no longer included in the container).
  Orphans remove(X xElement, Y yElement)
  {
    xOrphans_.clear();
    yOrphans_.clear();

    if (xToYListMap_.find(xElement) == xToYListMap_.end() || yToXListMap_.find(yElement) == yToXListMap_.end())
    {
      return {xOrphans_, yOrphans_};
    }

    xToYListMap_[xElement].remove(yElement);
    if (xToYListMap_[xElement].empty())
    {
      xOrphans_.push_back(xElement);
      xToYListMap_.erase(xElement);
    }

    yToXListMap_[yElement].remove(xElement);
    if (yToXListMap_[yElement].empty())
    {
      yOrphans_.push_back(yElement);
      yToXListMap_.erase(yElement);
    }
    return {xOrphans_, yOrphans_};
  }

  /// Removes all the appearances of element X if it is included in the container.
  /// Returns orphan elements (elements that are no longer included in the container).
  Orphans remove(X xElement)
  {
    if (xToYListMap_.find(xElement) != xToYListMap_.end())
    {
      xOrphans_.clear();
      yOrphans_.clear();

      for (auto yElement: xToYListMap_[xElement])
      {
        yToXListMap_[yElement].remove(xElement);
        if (yToXListMap_[yElement].empty())
        {
          yOrphans_.push_back(yElement);
          yToXListMap_.erase(yElement);
        }
      }
      xOrphans_.push_back(xElement);
      xToYListMap_.erase(xElement);
    }
    return {xOrphans_, yOrphans_};
  }

  /// Removes all the appearances of element Y if it is included in the container.
  /// Returns orphan elements (elements that are no longer included in the container).
  Orphans remove(Y yElement)
  {
    if (yToXListMap_.find(yElement) != yToXListMap_.end())
    {
      xOrphans_.clear();
      yOrphans_.clear();
      for (auto xElement: yToXListMap_[yElement])
      {
        xToYListMap_[xElement].remove(yElement);
        if (xToYListMap_[xElement].empty())
        {
          xOrphans_.push_back(xElement);
          xToYListMap_.erase(xElement);
        }
      }
      yOrphans_.push_back(yElement);
      yToXListMap_.erase(yElement);
    }
    return {xOrphans_, yOrphans_};
  }

  /// Applies the given function object fun to the elements [xElement, yList] in the xToYListMap.
  template <typename Fn, typename std::enable_if_t<std::is_invocable_v<Fn, X&, const List<Y>&>, int> = 0>
  void forEach(Fn&& fun)
  {
    for (auto& [xElement, yList]: xToYListMap_)
    {
      fun(xElement, yList);
    }
  }

  /// Applies the given function object fun to the elements [yElement, xList] in the yToXListMap.
  template <typename Fn, typename std::enable_if_t<std::is_invocable_v<Fn, Y&, const List<X>&>, int> = 0>
  void forEach(Fn&& fun)
  {
    for (auto& [yElement, xList]: yToXListMap_)
    {
      fun(yElement, xList);
    }
  }

  /// Applies the given function object fun to the elements [xElement, yElement] in the xToYListMap.
  template <typename Fn, typename std::enable_if_t<std::is_invocable_v<Fn, X&, Y&>, int> = 0>
  void forEach(Fn&& fun)
  {
    for (auto& [xElement, yList]: xToYListMap_)
    {
      for (auto& yElement: yList)
      {
        fun(xElement, yElement);
      }
    }
  }

  /// Applies the given function object fun to the elements [yElement, xElement] in the yToXListMap.
  template <typename Fn, typename std::enable_if_t<std::is_invocable_v<Fn, Y&, X&>, int> = 0>
  void forEach(Fn&& fun)
  {
    for (auto& [yElement, xList]: yToXListMap_)
    {
      for (auto& xElement: xList)
      {
        fun(yElement, xElement);
      }
    }
  }

  /// Applies the given function object fun to the elements [yElement] in the xToYListMap[xElement].
  template <typename Fn, typename = std::enable_if_t<std::is_invocable_v<Fn, Y&>>>
  void forEach(X xElement, Fn&& fun)
  {
    if (!contains(xElement))
    {
      return;
    }

    for (auto& yElement: xToYListMap_[xElement])
    {
      fun(yElement);
    }
  }

  /// Applies the given function object fun to the elements [xElement] in the yToXListMap[yElement].
  template <typename Fn, typename = std::enable_if_t<std::is_invocable_v<Fn, X&>>>
  void forEach(Y yElement, Fn&& fun)
  {
    if (!contains(yElement))
    {
      return;
    }

    for (auto& xElement: yToXListMap_[yElement])
    {
      fun(xElement);
    }
  }

  /// Erases all the elements.
  void clear()
  {
    xToYListMap_.clear();
    yToXListMap_.clear();
  }

public:  // lookup
  /// Access specified element. Returns nullptr if not present.
  [[nodiscard]] const List<Y>* tryGet(X xElement) const noexcept
  {
    auto itr = xToYListMap_.find(xElement);
    if (itr == xToYListMap_.end())
    {
      return nullptr;
    }
    return &(itr->second);
  }

  /// Access specified element. Returns nullptr if not present.
  [[nodiscard]] const List<X>* tryGet(Y yElement) const noexcept
  {
    auto itr = yToXListMap_.find(yElement);
    if (itr == yToXListMap_.end())
    {
      return nullptr;
    }
    return &(itr->second);
  }

  /// Returns true if the container contains X element.
  [[nodiscard]] bool contains(X xElement) const { return xToYListMap_.find(xElement) != xToYListMap_.end(); }

  /// Returns true if the container contains Y element.
  [[nodiscard]] bool contains(Y yElement) const { return yToXListMap_.find(yElement) != yToXListMap_.end(); }

private:
  std::unordered_map<X, List<Y>> xToYListMap_;
  std::unordered_map<Y, List<X>> yToXListMap_;
  std::vector<X> xOrphans_;
  std::vector<Y> yOrphans_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_CONTAINERS_H
