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

/// FixedString class that implements a std::string / std::string_view compatible interface.
/// For detailed documentation checkout the interface documentation of one of those classes.
template <typename CharT, size_t maxCapacity, typename Traits = std::char_traits<CharT>>
class FixedStringBase
{
public:
  using traits_type = Traits;
  using value_type = CharT;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
  static constexpr size_type npos {std::numeric_limits<size_type>::max()};

public:
  constexpr FixedStringBase() = default;
  explicit constexpr FixedStringBase(const CharT* s): FixedStringBase(std::string_view(s)) {}
  explicit constexpr FixedStringBase(const std::string& s): FixedStringBase(std::string_view(s)) {};
  explicit constexpr FixedStringBase(std::string_view s) { assignRange(util::makeRange(s.begin(), s.end())); };

public:  // Basic member functions
  template <typename CharT2 = CharT, size_t capacity2>
  FixedStringBase& operator=(const FixedStringBase<CharT2, capacity2>& other)
  {
    assignRange(util::makeRange(other.begin(), other.end()));
    return *this;
  }
  FixedStringBase& operator=(const CharT* s)
  {
    FixedStringBase::operator=(std::string_view {s});
    return *this;
  }
  template <class StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& operator=(const StringViewLike& t)
  {
    assignRange(util::makeRange(t.begin(), t.end()));
    return *this;
  }
  FixedStringBase& operator=(std::nullptr_t) = delete;

  FixedStringBase& assign(const std::string& s)
  {
    assignRange(util::makeRange(s.begin(), s.end()));
    return *this;
  }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& assign(const StringViewLike& s)
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

public:  // Element access
  [[nodiscard]] CharT& at(size_t idx)
  {
    if (idx >= size())
    {
      throw std::out_of_range("FixedString::at: " + std::to_string(idx) + " is out of range for size " +
                              std::to_string(size()));
    }
    return operator[](idx);
  }
  [[nodiscard]] const CharT& at(size_t idx) const
  {
    if (idx >= size())
    {
      throw std::out_of_range("FixedString::at: " + std::to_string(idx) + " is out of range for size " +
                              std::to_string(size()));
    }
    return operator[](idx);
  }
  [[nodiscard]] CharT& operator[](size_t idx)
  {
    checkIdxInRange(idx);
    return data_[idx];
  }
  [[nodiscard]] const CharT& operator[](size_t idx) const
  {
    checkIdxInRange(idx);
    return data_[idx];
  }

  [[nodiscard]] CharT& front()
  {
    checkIdxInRange(0);
    return operator[](0);
  }
  [[nodiscard]] const CharT& front() const
  {
    checkIdxInRange(0);
    return operator[](0);
  }

  [[nodiscard]] CharT& back()
  {
    checkIdxInRange(0);
    return operator[](capacity() - 1);
  }
  [[nodiscard]] const CharT& back() const
  {
    checkIdxInRange(0);
    return operator[](capacity() - 1);
  }

  [[nodiscard]] CharT* data() noexcept { return data_.data(); }
  [[nodiscard]] const CharT* data() const noexcept { return data_.data(); }

  [[nodiscard]] const CharT* c_str() const noexcept { return data(); }

public:  // Iterators
  iterator begin() { return data_.begin(); }
  const_iterator begin() const { return data_.cbegin(); }
  const_iterator cbegin() const noexcept { return data_.cbegin(); }

  iterator end() { return std::next(data_.begin(), usedSize_); }
  const_iterator end() const { return std::next(data_.cbegin(), usedSize_); }
  const_iterator cend() const noexcept { return std::next(data_.cbegin(), usedSize_); }

  reverse_iterator rbegin() { return std::next(data_.rbegin(), 1); }
  const_reverse_iterator rbegin() const { return std::next(data_.crbegin(), 1); }
  const_reverse_iterator crbegin() const noexcept { return std::next(data_.crbegin(), 1); }

  reverse_iterator rend() { return data_.rend(); }
  const_reverse_iterator rend() const { return data_.crend(); }
  const_reverse_iterator crend() const noexcept { return data_.crend(); }

public:  // Capacity
  [[nodiscard]] constexpr bool empty() const noexcept { return usedSize_ == 0U; }
  [[nodiscard]] constexpr size_t size() const noexcept { return usedSize_; }
  [[nodiscard]] constexpr size_t length() const noexcept { return usedSize_; }
  [[nodiscard]] constexpr size_t capacity() const noexcept { return maxCapacity; }

  //--------------------------------------------------------------------------//
  // Modifiers

  void clear() { usedSize_ = 0; }

  FixedStringBase& insert(size_type idx, size_type count, CharT ch)
  {
    return insert(convertToIterator(idx), count, ch);
  }
  FixedStringBase& insert(size_type idx, const CharT* s) { return insert(idx, std::string_view {s}); }
  FixedStringBase& insert(size_type idx, const CharT* s, size_type count)
  {
    return insert(idx, std::string_view {s, count});
  }
  FixedStringBase& insert(const_iterator pos, CharT ch) { return insert(pos, size_type {1}, ch); }
  FixedStringBase& insert(const_iterator pos, size_type count, CharT ch)
  {
    makeGap(pos, count);
    std::fill_n(std::next(begin(), convertToIndex(pos)), count, ch);
    return *this;
  }
  template <typename InputIt>
  FixedStringBase& insert(const_iterator pos, InputIt first, InputIt last)
  {
    return insert(convertToIndex(pos), std::string_view {first, last});
  }
  FixedStringBase& insert(const_iterator pos, std::initializer_list<CharT> initList)
  {
    return insert(convertToIndex(pos), std::string_view {initList.begin(), initList.end()});
  }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& insert(size_type idx, const StringViewLike& sv)
  {
    makeGap(idx, sv.length());
    insertRange(util::makeRange(sv), idx);
    return *this;
  }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& insert(size_type idx, const StringViewLike& sv, size_type svIdx, size_type count = npos)
  {
    return insert(idx, std::string_view {sv}.substr(svIdx, count));
  }

  FixedStringBase& erase(size_t idx = 0, size_t count = npos)
  {
    makeDestructiveMoveLeft(convertToIterator(idx), count != npos ? count : size() - idx);
    return *this;
  }
  FixedStringBase& erase(const_iterator pos)
  {
    makeDestructiveMoveLeft(pos, 1);
    return *this;
  }
  FixedStringBase& erase(const_iterator first, const_iterator last)
  {
    makeDestructiveMoveLeft(first, std::distance(first, last));
    return *this;
  }

  constexpr void push_back(CharT ch) { append(1, ch); }

  constexpr void pop_back()
  {
    SEN_DEBUG_ASSERT(!empty());
    usedSize_--;
  }

  FixedStringBase& append(size_type count, CharT ch) { return insert(usedSize_, count, ch); }
  FixedStringBase& append(const CharT* s, size_type count) { return insert(usedSize_, std::string_view {s, count}); }
  FixedStringBase& append(const CharT* s) { return insert(usedSize_, s); }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& append(const StringViewLike& t)
  {
    return insert(usedSize_, t);
  }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& append(const StringViewLike& t, size_type pos, size_type count = npos)
  {
    return insert(usedSize_, t, pos, count);
  }
  template <typename InputIt>
  FixedStringBase& append(InputIt first, InputIt last)
  {
    return insert(end(), first, last);
  }
  FixedStringBase& append(std::initializer_list<CharT> initList) { return insert(end(), std::move(initList)); }

  FixedStringBase& operator+=(CharT ch) { return append(1, ch); }
  FixedStringBase& operator+=(const CharT* s) { return append(s); }
  FixedStringBase& operator+=(std::initializer_list<CharT> initList) { return append(std::move(initList)); }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& operator+=(const StringViewLike& t)
  {
    return append(t);
  }

  FixedStringBase& replace(size_type pos, size_type count, const CharT* cstr, size_type count2)
  {
    return replace(pos, count, std::string_view {cstr, count2});
  }
  FixedStringBase& replace(const_iterator first, const_iterator last, const CharT* cstr, size_type count2)
  {
    return replace(first, last, std::string_view {cstr, count2});
  }
  FixedStringBase& replace(size_type pos, size_type count, const CharT* cstr)
  {
    return replace(convertToIterator(pos), convertToIterator(pos + count), std::string_view {cstr});
  }
  FixedStringBase& replace(const_iterator first, const_iterator last, const CharT* cstr)
  {
    return replace(first, last, std::string_view {cstr});
  }
  FixedStringBase& replace(size_type pos, size_type count, size_type count2, CharT ch)
  {
    throwWhenPositionOutOfRange(pos);
    std::fill_n(std::next(begin(), pos), std::min(count, count2), ch);
    return *this;
  }
  FixedStringBase& replace(const_iterator first, const_iterator last, size_type count2, CharT ch)
  {
    return replace(convertToIndex(first), convertToIndex(last) - convertToIndex(first), count2, ch);
  }
  template <typename InputIt>
  FixedStringBase& replace(const_iterator first, const_iterator last, InputIt first2, InputIt last2)
  {
    return replace(first, last, std::string_view {first2, last2});
  }
  FixedStringBase& replace(const_iterator first, const_iterator last, std::initializer_list<CharT> initList)
  {
    return replace(first, last, std::string_view {initList.begin(), initList.end()});
  }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& replace(size_type pos, size_type count, const StringViewLike& t)
  {
    throwWhenPositionOutOfRange(pos);
    makeInsertion(util::makeRange(t.begin(), std::next(t.begin(), std::min(count, t.size()))), pos);
    return *this;
  }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& replace(const_iterator first, const_iterator last, const StringViewLike& t)
  {
    return replace(convertToIndex(first), convertToIndex(last), t);
  }
  template <typename StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  FixedStringBase& replace(size_type pos,
                           size_type count,
                           const StringViewLike& t,
                           size_type pos2,
                           size_type count2 = npos)
  {
    return replace(pos, count, t.substr(pos2, count2));
  }

  size_type copy(CharT* dest, size_type count, size_t pos = 0) const
  {
    throwWhenPositionOutOfRange(pos);
    size_type numCharsToCopy = std::min(count - pos, size());
    std::copy_n(std::next(begin(), pos), numCharsToCopy, dest);
    return numCharsToCopy;
  }

  constexpr void swap(FixedStringBase& other)
  {
    StorageType tmpData = data_;
    data_ = other.data_;
    other.data_ = tmpData;

    size_t tmpSize = usedSize_;
    usedSize_ = other.usedSize_;
    other.usedSize_ = tmpSize;
  }

public:  // Search
  size_type find(const CharT* s, size_type pos, size_type count) const noexcept { return view().find(s, pos, count); }
  size_type find(const CharT* s, size_type pos = 0) const noexcept { return view().find(s, pos); }
  size_type find(CharT ch, size_type pos = 0) const noexcept { return view().find(ch, pos); };
  template <class StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  size_type find(const StringViewLike& t, size_type pos = 0) const noexcept
  {
    return view().find(t, pos);
  }

  size_type rfind(const CharT* s, size_type pos, size_type count) const noexcept { return view().rfind(s, pos, count); }
  size_type rfind(const CharT* s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
  size_type rfind(CharT ch, size_type pos = npos) const noexcept { return view().rfind(ch, pos); };
  template <class StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
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
  template <class StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
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
  template <class StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
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
  template <class StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
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
  template <class StringViewLike,
            std::enable_if_t<std::is_convertible_v<StringViewLike, std::string_view>, bool> = true>
  size_type find_last_not_of(const StringViewLike& t, size_type pos = npos) const noexcept
  {

    return view().find_last_not_of(t, pos);
  }

public:  // Operations
  int compare(const FixedStringBase& str) const { return view().compare(str.view()); }

#ifdef __cpp_lib_starts_ends_with
  constexpr bool starts_with(std::basic_string_view<CharT, Traits> sv) const noexcept { return view().starts_with(sv); }
  constexpr bool starts_with(CharT ch) const noexcept { return view().starts_with(ch); }
  constexpr bool starts_with(const CharT* s) const { return view().starts_with(s); }

  constexpr bool ends_with(std::basic_string_view<CharT, Traits> sv) const noexcept { return view().ends_with(sv); }
  constexpr bool ends_with(CharT ch) const noexcept { return view().ends_with(ch); }
  constexpr bool ends_with(const CharT* s) const { return view().ends_with(s); }
#endif

#ifdef __cpp_lib_string_contains
  constexpr bool contains(std::basic_string_view<CharT, Traits> sv) const noexcept { return view().contains(sv); }
  constexpr bool contains(CharT ch) const noexcept { return view().contains(ch); }
  constexpr bool contains(const CharT* s) const { return view().contains(s); }
#endif

  FixedStringBase substr(size_type pos = 0, size_type count = npos) const { return view().substr(pos, count); }

  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  constexpr operator std::basic_string_view<CharT>() const noexcept { return view(); }

  friend constexpr bool operator==(const FixedStringBase& lhs, const FixedStringBase& rhs)
  {
    return lhs.size() == rhs.size() && !Traits::compare(lhs.data_.data(), rhs.data_.data(), lhs.usedSize_);
  }
  friend constexpr bool operator==(const FixedStringBase& lhs, std::string_view rhs)
  {
    return lhs.size() == rhs.size() && !Traits::compare(lhs.data_.data(), rhs.data(), lhs.usedSize_);
  }
  friend constexpr bool operator==(std::string_view lhs, const FixedStringBase& rhs)
  {
    return lhs.size() == rhs.size() && !Traits::compare(lhs.data(), rhs.data_.data(), rhs.usedSize_);
  }
  friend constexpr bool operator==(const FixedStringBase& lhs, const CharT* rhs)
  {
    return lhs.size() == Traits::length(rhs) && !Traits::compare(lhs.data_.data(), rhs, lhs.usedSize_);
  }
  friend constexpr bool operator==(const CharT* lhs, const FixedStringBase& rhs)
  {
    return Traits::length(lhs) == rhs.size() && !Traits::compare(lhs, rhs.data_.data(), Traits::length(lhs));
  }

  friend constexpr bool operator!=(const FixedStringBase& lhs, const FixedStringBase& rhs) { return !(lhs == rhs); }
  friend constexpr bool operator!=(const FixedStringBase& lhs, std::string_view rhs) { return !(lhs == rhs); }
  friend constexpr bool operator!=(std::string_view lhs, const FixedStringBase& rhs) { return !(lhs == rhs); }
  friend constexpr bool operator!=(const FixedStringBase& lhs, const CharT* rhs) { return !(lhs == rhs); }
  friend constexpr bool operator!=(const CharT* lhs, const FixedStringBase& rhs) { return !(lhs == rhs); }

private:
  using StorageType = std::array<CharT, maxCapacity + 1>;
  using IteratorType = typename StorageType::iterator;
  using ConstIteratorType = typename StorageType::const_iterator;

  template <typename T>
  constexpr void insertRange(sen::util::IteratorRange<T> charRange, size_type idx = 0)
  {
    size_type idxAfterInsertion = makeInsertion(charRange, idx);
    usedSize_ = std::max(idxAfterInsertion, usedSize_);
    data_[usedSize_] = '\0';
  }

  template <typename T>
  constexpr void assignRange(sen::util::IteratorRange<T> charRange, size_type idx = 0)
  {
    size_type idxAfterInsertion = makeInsertion(charRange, idx);
    usedSize_ = idxAfterInsertion;
    data_[usedSize_] = '\0';
  }

  [[nodiscard]] constexpr std::basic_string_view<CharT> view() const noexcept { return {data_.data(), usedSize_}; }

  [[nodiscard]] constexpr bool isWithinCapacity(size_t idx) const noexcept { return idx < capacity(); }
  [[nodiscard]] constexpr bool isNotWithinCapacity(size_t idx) const noexcept { return !isWithinCapacity(idx); }
  [[nodiscard]] constexpr iterator convertToIterator(size_type idx) noexcept { return std::next(begin(), idx); }
  [[nodiscard]] constexpr size_type convertToIndex(const_iterator iter) const noexcept
  {
    return std::distance(cbegin(), iter);
  }

  void throwWhenPositionOutOfRange(size_t pos) const
  {
    if (pos > size())
    {
      throw std::out_of_range("Pos (" + std::to_string(pos) + ") was out of range.");
    }
  }

  //===------------------------------------------------------------------------------------------------------------===//
  // user input based mutating functions: ensure that every make* functions validates it's input
  constexpr void checkIdxInRange(size_type idx) const noexcept
  {
    SEN_DEBUG_ASSERT(idx >= 0 && usedSize_ <= idx && "Index was out-of-bounds.");
    std::ignore = idx;
  }
  constexpr void checkIteratorRange(const_iterator iter) const noexcept
  {
    SEN_DEBUG_ASSERT(cbegin() <= iter && iter < cend() && "Iterator was out-of-bounds.");
    std::ignore = iter;
  }

  template <typename T>
  constexpr size_t makeInsertion(sen::util::IteratorRange<T> charRange, size_type idx = 0)
  {
    // Precondition checks
    checkIdxInRange(idx);
    SEN_DEBUG_ASSERT(idx + std::distance(charRange.begin(), charRange.end()) < maxCapacity + 1);

    for (CharT c: charRange)
    {
      if (isNotWithinCapacity(idx))  // ensures that even in release builds, we don't write out of bounds
      {
        break;
      }
      data_[idx] = c;
      idx++;
    }
    return idx;
  }

  constexpr void makeGap(size_type idx, size_type amount) { makeGap(convertToIterator(idx), amount); }
  constexpr void makeGap(const_iterator pos, size_type amount)
  {
    // Required pre checks
    if (amount + size() > capacity())
    {
      throw std::length_error("Number of inserted characters exceeds capacity");
    }

    // Precondition checks
    checkIteratorRange(pos);
    checkIteratorRange(std::next(end(), amount));

    std::move_backward(pos, cend(), std::next(end(), amount));
    usedSize_ += amount;
  }

  constexpr void makeDestructiveMoveLeft(size_type idx, size_type count)
  {
    // Precondition checks
    checkIdxInRange(idx);

    std::move(std::next(convertToIterator(idx), count), end(), convertToIterator(idx));
    usedSize_ -= count;
  }
  constexpr void makeDestructiveMoveLeft(const_iterator pos, size_type count)
  {
    makeDestructiveMoveLeft(convertToIndex(pos), count);
  }

private:  // Data members
  StorageType data_ {'\0'};
  size_type usedSize_ {0};
};

template <size_t maxSize>
using FixedString = FixedStringBase<char, maxSize>;

}  // namespace sen
// NOLINTEND(readability-identifier-naming)

#endif  // SEN_CORE_BASE_FIXED_STRING_H
