// === span.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_SPAN_H
#define SEN_CORE_BASE_SPAN_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"

// std
#include <array>
#include <iterator>
#include <vector>

namespace sen
{

/// \addtogroup util
/// @{

/// Contiguous view of elements of type T.
/// Inspired by http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0122r7.pdf
///
/// @warning this class does not perform range checks.
///
/// @tparam T element type.
template <class T>
class Span
{
public:
  SEN_COPY_MOVE(Span)

public:
  using element_type = T;                                                // NOLINT
  using value_type = std::remove_cv_t<T>;                                // NOLINT
  using index_type = std::size_t;                                        // NOLINT
  using difference_type = ptrdiff_t;                                     // NOLINT
  using pointer = T*;                                                    // NOLINT
  using reference = T&;                                                  // NOLINT
  using iterator = T*;                                                   // NOLINT
  using const_iterator = const T*;                                       // NOLINT
  using reverse_iterator = std::reverse_iterator<iterator>;              // NOLINT
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;  // NOLINT

public:  // defaulted members
  Span() noexcept = default;
  ~Span() noexcept = default;

public:  // constructors
  /// Copy from a compatible Span
  template <typename U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
  constexpr Span(const Span<U>& other) noexcept  // NOLINT(hicpp-explicit-conversions)
    : data_(other.data_), size_(other.size_)
  {
  }

  /// Builds a Span out of a pointer and an element count.
  /// @pre (ptr == nullptr && count == 0) || (ptr != nullptr && count >= 0)
  constexpr Span(pointer ptr, index_type count) noexcept: data_(ptr), size_(count) {}

  /// Builds a Span out of an array.
  template <std::size_t n>  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  constexpr Span(std::array<value_type, n>& array) noexcept: data_(array.data()), size_(array.size())
  {
  }

  /// Builds a Span out of an array.
  template <std::size_t n>  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  constexpr Span(const std::array<value_type, n>& array) noexcept: data_(array.data()), size_(array.size())
  {
  }

  /// Builds a Span out of an array.
  template <typename U, std::size_t n, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
  constexpr Span(std::array<U, n>& array) noexcept  // NOLINT(hicpp-explicit-conversions)
    : data_(array.data()), size_(array.size())
  {
  }

  /// Builds a Span out of a const array.
  template <typename U, std::size_t n, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
  constexpr Span(const std::array<U, n>& array) noexcept  // NOLINT(hicpp-explicit-conversions)
    : data_(array.data()), size_(array.size())
  {
  }

  /// Builds a Span out of a vector. NOLINTNEXTLINE(hicpp-explicit-conversions)
  constexpr Span(std::vector<value_type>& vector) noexcept: data_(vector.data()), size_(vector.size()) {}

  /// Builds a Span out of a vector. NOLINTNEXTLINE(hicpp-explicit-conversions)
  constexpr Span(const std::vector<value_type>& vector) noexcept: data_(vector.data()), size_(vector.size()) {}

  /// Builds a Span out of a compatible vector.
  template <typename U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
  constexpr Span(std::vector<U>& vector) noexcept  // NOLINT(hicpp-explicit-conversions)
    : data_(vector.data()), size_(vector.size())
  {
  }

  /// Builds a Span out of a const compatible vector.
  template <typename U, std::enable_if_t<std::is_convertible_v<U*, T*>, int> = 0>
  constexpr Span(const std::vector<U>& vector) noexcept  // NOLINT(hicpp-explicit-conversions)
    : data_(vector.data()), size_(vector.size())
  {
  }

public:  // subviews
  /// Returns a new span over the first count elements of *this
  /// @pre count >= 0 && count <= size()
  [[nodiscard]] Span first(index_type count) const noexcept;

  /// Returns a new Span over the last count elements of *this
  /// @pre count >= 0 && count <= size()
  [[nodiscard]] Span last(index_type count) const noexcept;

  /// Returns a new Span over the elements of *this beginning at offset and
  /// extending for size() - offset elements.
  /// @pre offset >= 0 && offset <= size()
  [[nodiscard]] Span subspan(index_type offset = 0) const noexcept;

  /// Returns a new Span over the elements of *this beginning at offset and
  /// extending for count elements.
  /// @pre offset >= 0 && count <= size() - offset
  [[nodiscard]] Span subspan(index_type offset, index_type count) const noexcept;

public:  // observers
  /// Number of elements covered by the Span.
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

  /// @return true if size() == 0, false otherwise.
  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

public:  // element access
  /// Gets the element at index i.
  /// Complexity: constant
  /// @pre i < size()
  /// @pre i >= 0
  ///
  /// @warning This function does not perform range checks. Please
  ///          ensure that i is within valid limits.
  ///
  /// @param i index of the element to fetch.
  /// @return the value of the element at position i.
  [[nodiscard]] reference operator[](index_type i) const noexcept;

  /// Direct access to the underlying storage.
  /// Complexity: constant
  [[nodiscard]] constexpr pointer data() const noexcept { return data_; }

public:  // iterator support
  /// Returns an iterator to the first element of the Span.
  /// If the vector is empty, the returned iterator will be equal to end().
  /// Complexity: constant.
  [[nodiscard]] constexpr iterator begin() const noexcept { return {data()}; }

  /// Iterator to the element following the last element.
  /// Complexity: constant. NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  [[nodiscard]] constexpr iterator end() const noexcept { return {data() + size()}; }

  /// Returns a const iterator to the first element of the vector.
  /// If the Span is empty, the returned iterator will be equal to cend().
  /// Complexity: constant.
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return {data()}; }

  /// Constant iterator to the element following the last element.
  /// Complexity: constant. NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return {data() + size()}; }

private:
  pointer data_ = nullptr;
  index_type size_ = 0U;
};

//--------------------------------------------------------------------------------------------------------------
// Comparison operators
//--------------------------------------------------------------------------------------------------------------

template <typename T>
[[nodiscard]] constexpr bool operator==(const Span<T>& lhs, const Span<T>& rhs) noexcept
{
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <typename T>
[[nodiscard]] constexpr bool operator!=(const Span<T>& lhs, const Span<T>& rhs) noexcept
{
  return !(lhs == rhs);
}

//--------------------------------------------------------------------------------------------------------------
// makeSpan
//--------------------------------------------------------------------------------------------------------------

/// Takes in a type that can be passed to a contiguous range and returns a Span.
template <typename T>
[[nodiscard]] constexpr Span<T> makeSpan(T* ptr, std::size_t size) noexcept
{
  return Span<T>(ptr, size);
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
template <typename T, std::size_t n>
[[nodiscard]] constexpr Span<T> makeSpan(T (&arr)[n]) noexcept
{
  return makeSpan(arr, n);
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
template <typename T>
[[nodiscard]] constexpr Span<T> makeSpan(T& val) noexcept
{
  return makeSpan(&val, 1UL);
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
template <typename Iterator, typename T = typename std::iterator_traits<Iterator>::value_type>
[[nodiscard]] constexpr Span<T> makeSpan(Iterator first, Iterator last) noexcept
{
  return makeSpan(&*first, static_cast<std::size_t>(std::distance(first, last)));
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
template <typename T>
[[nodiscard]] constexpr Span<T> makeSpan(std::vector<T>& vector) noexcept
{
  return Span<T>(vector);
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
template <typename T, std::size_t s>
[[nodiscard]] constexpr Span<T> makeSpan(std::array<T, s>& array) noexcept
{
  return Span<T>(array);
}

//--------------------------------------------------------------------------------------------------------------
// makeConstSpan
//--------------------------------------------------------------------------------------------------------------

/// Takes in a type that can be passed to a contiguous range and returns a Span.
/// The element types will be const qualified.
template <typename T>
[[nodiscard]] constexpr Span<const T> makeConstSpan(const T* ptr, std::size_t size) noexcept
{
  return Span<const T>(ptr, size);
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
/// The element types will be const qualified.
template <typename T, std::size_t n>
[[nodiscard]] constexpr Span<const T> makeConstSpan(const T (&arr)[n]) noexcept
{
  return makeConstSpan(arr, n);
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
/// The element types will be const qualified.
template <typename Iterator, typename T = typename std::iterator_traits<Iterator>::value_type>
[[nodiscard]] constexpr Span<const T> makeConstSpan(Iterator first, Iterator last) noexcept
{
  return makeConstSpan(&*first, static_cast<std::size_t>(std::distance(first, last)));
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
/// The element types will be const qualified.
template <typename T>
[[nodiscard]] constexpr Span<const T> makeConstSpan(const std::vector<T>& vector) noexcept
{
  return Span<const T>(vector);
}

/// Takes in a type that can be passed to a contiguous range and returns a Span.
/// The element types will be const qualified.
template <typename T, std::size_t s>
[[nodiscard]] constexpr Span<const T> makeConstSpan(const std::array<T, s>& array) noexcept
{
  return Span<const T>(array);
}

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
Span<T> Span<T>::first(index_type count) const noexcept
{
  SEN_EXPECT(count <= size());
  return {data(), count};
}

template <typename T>
Span<T> Span<T>::last(index_type count) const noexcept
{
  SEN_EXPECT(count <= size());
  return {data() + (size() - count), count};  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

template <typename T>
Span<T> Span<T>::subspan(index_type offset) const noexcept
{
  SEN_EXPECT(offset <= size());
  return {data() + offset, size() - offset};  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

template <typename T>
Span<T> Span<T>::subspan(index_type offset, std::size_t count) const noexcept
{
  SEN_EXPECT(offset <= size());
  SEN_EXPECT(count <= (size() - offset));
  return {data() + offset, count};  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

template <typename T>
typename Span<T>::reference Span<T>::operator[](index_type i) const noexcept
{
  SEN_EXPECT(i < size());
  return *(data() + i);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

}  // namespace sen

#endif  // SEN_CORE_BASE_SPAN_H
