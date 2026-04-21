// === fixed_string.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_FIXED_STRING_H
#define SEN_CORE_BASE_FIXED_STRING_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/iterator_adapters.h"

// std
#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace sen
{

// NOLINTBEGIN(readability-identifier-naming)

template <typename CharT, size_t maxCapacity, typename Traits = std::char_traits<CharT>>
class FixedStringBase
{
public:
  using traits_type = Traits;
  using value_type = CharT;
  using size_type = size_t;
  // difference_type
  using reference = value_type&;
  using const_reference = const value_type&;
  // pointer
  // const_pointer
  // iterator
  // const_iterator
  // reverse_iterator
  // const_reverse_iterator

public:
  static constexpr size_type npos {std::numeric_limits<size_type>::max()};

public:
  FixedStringBase() = default;
  explicit FixedStringBase(const CharT* s): FixedStringBase(std::string_view(s)) {}
  explicit FixedStringBase(const std::string& s): FixedStringBase(std::string_view(s)) {};
  explicit FixedStringBase(std::string_view s) { assignRange(util::makeRange(s.begin(), s.end())); };

  //------------------------------------------------------------------------------------------------------------------//
  // Basic member functions

  template <typename CharT2 = CharT, size_t capacity2>
  FixedStringBase& operator=(const FixedStringBase<CharT2, capacity2>& other)
  {
    assignRange(util::makeRange(other.begin(), other.end()));
    return *this;
  }

  // TODO insert other operator=s

  FixedStringBase& assign(const std::string& s)
  {
    assignRange(util::makeRange(s.begin(), s.end()));
    return *this;
  }

  template <typename SV>
  FixedStringBase& assign(const SV& s)
  {
    assignRange(util::makeRange(s.begin(), s.end()));
    return *this;
  }

  template <typename InputIt>
  FixedStringBase& assign(InputIt first, InputIt last)
  {
    assignRange(util::makeRange(first, last));
    return *this;
  }

  FixedStringBase& assign(std::initializer_list<CharT> initList)
  {
    assignRange(util::makeRange(initList.begin(), initList.end()));
    return *this;
  }

  //------------------------------------------------------------------------------------------------------------------//
  // Element access

  [[nodiscard]] CharT& at(size_t idx)
  {
    if (idx >= usedSize_)
    {
      throw std::out_of_range("FixedString::at: ");
      // TODO insert data
      // basic_string::at: __n "
      // "(which is %zu) >= this->size() "
      // "(which is %zu)"
    }
    return operator[](idx);
  }

  [[nodiscard]] const CharT& at(size_t idx) const
  {
    if (idx >= usedSize_)
    {
      throw std::out_of_range("FixedString::at: ");
      // TODO insert data
      // basic_string::at: __n "
      // "(which is %zu) >= this->size() "
      // "(which is %zu)"
    }
    return operator[](idx);
  }

  [[nodiscard]] CharT& operator[](size_t idx)
  {
    SEN_DEBUG_ASSERT(isWithinCapacity(idx));
    return data_[idx];
  }

  [[nodiscard]] const CharT& operator[](size_t idx) const
  {
    SEN_DEBUG_ASSERT(isWithinCapacity(idx));
    return data_[idx];
  }

  [[nodiscard]] CharT& front()
  {
    SEN_DEBUG_ASSERT(!empty());
    return operator[](0);
  }

  [[nodiscard]] const CharT& front() const
  {
    SEN_DEBUG_ASSERT(!empty());
    return operator[](0);
  }

  [[nodiscard]] CharT& back()
  {
    SEN_DEBUG_ASSERT(!empty());
    return operator[](capacity() - 1);
  }

  [[nodiscard]] const CharT& back() const
  {
    SEN_DEBUG_ASSERT(!empty());
    return operator[](capacity() - 1);
  }

  [[nodiscard]] CharT* data() noexcept { return data_.data(); }
  [[nodiscard]] const CharT* data() const noexcept { return data_.data(); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  [[nodiscard]] const CharT* c_str() const noexcept { return data(); }

  //------------------------------------------------------------------------------------------------------------------//
  // Iterators

  auto begin() { return data_.begin(); }
  auto begin() const { return data_.cbegin(); }
  auto cbegin() const noexcept { return data_.cbegin(); }

  auto end() { return std::next(data_.begin(), usedSize_); }
  auto end() const { return std::next(data_.cbegin(), usedSize_); }
  auto cend() const noexcept { return std::next(data_.cbegin(), usedSize_); }

  auto rbegin() { return std::next(data_.rbegin(), 1); }
  auto rbegin() const { return std::next(data_.crbegin(), 1); }
  auto crbegin() const noexcept { return std::next(data_.crbegin(), 1); }

  auto rend() { return data_.rend(); }
  auto rend() const { return data_.crend(); }
  auto crend() const noexcept { return data_.crend(); }

  //------------------------------------------------------------------------------------------------------------------//
  // Capacity

  [[nodiscard]] constexpr bool empty() const noexcept { return usedSize_ == 0U; }
  [[nodiscard]] constexpr size_t size() const noexcept { return usedSize_; }
  [[nodiscard]] constexpr size_t length() const noexcept { return usedSize_; }
  [[nodiscard]] constexpr size_t capacity() const noexcept { return maxCapacity; }

  //--------------------------------------------------------------------------//
  // Modifiers

  void clear() { usedSize_ = 0; }
  // TODO: insert
  FixedStringBase& erase(size_t index = 0, size_t count = npos) {}
  // TODO: erase
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr void push_back(CharT ch) { append(1, ch); }
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr void pop_back()
  {
    SEN_DEBUG_ASSERT(!empty());
    usedSize_--;
  }
  FixedStringBase& append(size_type count, CharT ch);
  // TODO: append

  FixedStringBase& operator+=(const FixedStringBase& str) { return append(str); }
  FixedStringBase& operator+=(CharT ch) { return append(1, ch); }
  FixedStringBase& operator+=(const CharT* s) { return append(s); }
  FixedStringBase& operator+=(std::initializer_list<CharT> initList) { return append(std::move(initList)); }
  template <typename StringViewLike>
  FixedStringBase& operator+=(const StringViewLike& t)
  {
    return append(t);
  }

  FixedStringBase& replace(size_type pos, size_type count, const FixedStringBase& str);  // TODO
  // TODO: replace

  size_type copy(CharT* dest, size_type count, size_t pos = 0) const
  {
    std::copy_n(std::next(begin(), pos), count, dest);
  }

  // TODO: swap
  // void swap(FixedStringBase& other)

  //------------------------------------------------------------------------------------------------------------------//
  // Search

  // TODO: review all constexpr

  size_type find(const CharT* s, size_type pos, size_type count) const noexcept { return view().find(s, pos, count); }
  size_type find(const CharT* s, size_type pos = 0) const noexcept { return view().find(s, pos); }
  size_type find(CharT ch, size_type pos = 0) const noexcept { return view().find(ch, pos); };
  template <class StringViewLike>
  size_type find(const StringViewLike& t, size_type pos = 0) const noexcept
  {
    return view().find(t, pos);
  }

  size_type rfind(const CharT* s, size_type pos, size_type count) const noexcept { return view().rfind(s, pos, count); }
  size_type rfind(const CharT* s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
  size_type rfind(CharT ch, size_type pos = npos) const noexcept { return view().rfind(ch, pos); };
  template <class StringViewLike>
  size_type rfind(const StringViewLike& t, size_type pos = npos) const noexcept
  {
    return view().rfind(t, pos);
  }

  size_type find_first_of(const CharT* s, size_type pos, size_type count) const noexcept
  {
    return view().find_first_of(s, pos, count);
  }
  size_type find_first_of(const CharT* s, size_type pos = 0) const noexcept { return view().find_first_of(s, pos); }
  size_type find_first_of(CharT ch, size_type pos = 0) const noexcept { return view().find_first_of(ch, pos); }
  template <class StringViewLike>
  size_type find_first_of(const StringViewLike& t, size_type pos = 0) const noexcept
  {

    return view().find_first_of(t, pos);
  }

  size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const noexcept
  {
    return view().find_first_not_of(s, pos, count);
  }
  size_type find_first_not_of(const CharT* s, size_type pos = 0) const noexcept
  {
    return view().find_first_not_of(s, pos);
  }
  size_type find_first_not_of(CharT ch, size_type pos = 0) const noexcept { return view().find_first_not_of(ch, pos); }
  template <class StringViewLike>
  size_type find_first_not_of(const StringViewLike& t, size_type pos = 0) const noexcept
  {

    return view().find_first_not_of(t, pos);
  }

  size_type find_last_of(const CharT* s, size_type pos, size_type count) const noexcept
  {
    return view().find_last_of(s, pos, count);
  }
  size_type find_last_of(const CharT* s, size_type pos = npos) const noexcept { return view().find_last_of(s, pos); }
  size_type find_last_of(CharT ch, size_type pos = npos) const noexcept { return view().find_last_of(ch, pos); }
  template <class StringViewLike>
  size_type find_last_of(const StringViewLike& t, size_type pos = npos) const noexcept
  {

    return view().find_last_of(t, pos);
  }

  size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const noexcept
  {
    return view().find_last_not_of(s, pos, count);
  }
  size_type find_last_not_of(const CharT* s, size_type pos = npos) const noexcept
  {
    return view().find_last_not_of(s, pos);
  }
  size_type find_last_not_of(CharT ch, size_type pos = npos) const noexcept { return view().find_last_not_of(ch, pos); }
  template <class StringViewLike>
  size_type find_last_not_of(const StringViewLike& t, size_type pos = npos) const noexcept
  {

    return view().find_last_not_of(t, pos);
  }

  //------------------------------------------------------------------------------------------------------------------//
  // Operations

  int compare(const FixedStringBase& str) const { return view().compare(str.view()); }

#ifdef __cpp_lib_starts_ends_with
  // TODO: write tests
  constexpr bool starts_with(std::basic_string_view<CharT, Traits> sv) const noexcept { return view().starts_with(sv); }
  constexpr bool starts_with(CharT ch) const noexcept { return view().starts_with(ch); }
  constexpr bool starts_with(const CharT* s) const { return view().starts_with(s); }

  constexpr bool ends_with(std::basic_string_view<CharT, Traits> sv) const noexcept { return view().ends_with(sv); }
  constexpr bool ends_with(CharT ch) const noexcept { return view().ends_with(ch); }
  constexpr bool ends_with(const CharT* s) const { return view().ends_with(s); }
#endif

#ifdef __cpp_lib_string_contains
  // TODO: write tests
  constexpr bool contains(std::basic_string_view<CharT, Traits> sv) const noexcept { return view().contains(sv); }
  constexpr bool contains(CharT ch) const noexcept { return view().contains(ch); }
  constexpr bool contains(const CharT* s) const { return view().contains(s); }
#endif

  FixedStringBase substr(size_type pos = 0, size_type count = npos) const { return view().substr(pos, count); }

  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  constexpr operator std::basic_string_view<CharT>() const noexcept { return view(); }

  friend bool operator==(const FixedStringBase& lhs, const FixedStringBase& rhs)
  {
    return lhs.size() == rhs.size() && !Traits::compare(lhs.data_.data(), rhs.data_.data(), lhs.usedSize_);
  }
  friend bool operator==(const FixedStringBase& lhs, std::string_view rhs)
  {
    return lhs.size() == rhs.size() && !Traits::compare(lhs.data_.data(), rhs.data(), lhs.usedSize_);
  }
  friend bool operator==(std::string_view lhs, const FixedStringBase& rhs)
  {
    return lhs.size() == rhs.size() && !Traits::compare(lhs.data(), rhs.data_.data(), rhs.usedSize_);
  }
  friend bool operator==(const FixedStringBase& lhs, const CharT* rhs)
  {
    return lhs.size() == Traits::length(rhs) && !Traits::compare(lhs.data_.data(), rhs, lhs.usedSize_);
  }
  friend bool operator==(const CharT* lhs, const FixedStringBase& rhs)
  {
    return Traits::length(lhs) == rhs.size() && !Traits::compare(lhs, rhs.data_.data(), Traits::length(lhs));
  }

  friend bool operator!=(const FixedStringBase& lhs, const FixedStringBase& rhs) { return !(lhs == rhs); }
  friend bool operator!=(const FixedStringBase& lhs, std::string_view rhs) { return !(lhs == rhs); }
  friend bool operator!=(std::string_view lhs, const FixedStringBase& rhs) { return !(lhs == rhs); }
  friend bool operator!=(const FixedStringBase& lhs, const CharT* rhs) { return !(lhs == rhs); }
  friend bool operator!=(const CharT* lhs, const FixedStringBase& rhs) { return !(lhs == rhs); }

private:
  using StorageType = std::array<CharT, maxCapacity + 1>;
  using IteratorType = typename StorageType::iterator;
  using ConstIteratorType = typename StorageType::const_iterator;

  // void assignRange(ConstIteratorType begin, ConstIteratorType end) { return assignRange({begin, end}); }
  template <typename T>
  void assignRange(sen::util::IteratorRange<T> charRange)
  {
    size_t idx {0};
    for (CharT c: charRange)
    {
      data_[idx] = c;
      idx++;
      if (isNotWithinCapacity(idx))
      {
        break;
      }
    }
    SEN_DEBUG_ASSERT(idx < maxCapacity + 1);
    usedSize_ = idx;
    data_[idx] = '\n';
  }

  [[nodiscard]] constexpr std::basic_string_view<CharT> view() const noexcept { return {data_.data(), usedSize_}; }

  [[nodiscard]] constexpr bool isWithinCapacity(size_t idx) const noexcept { return idx < capacity(); }
  [[nodiscard]] constexpr bool isNotWithinCapacity(size_t idx) const noexcept { return !isWithinCapacity(idx); }

  StorageType data_ {'\0'};
  size_type usedSize_ {0};
};

template <size_t maxSize>
using FixedString = FixedStringBase<char, maxSize>;

}  // namespace sen
// NOLINTEND(readability-identifier-naming)

#endif  // SEN_CORE_BASE_FIXED_STRING_H
