// === static_vector.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_STATIC_VECTOR_H
#define SEN_CORE_BASE_STATIC_VECTOR_H

#include "sen/core/base/assert.h"
#include "sen/core/base/result.h"

// std
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>

namespace sen
{

namespace impl
{

/// Gets the element at index i in the range rng and checks the bounds
template <typename T, typename Range, typename Index>
inline T& index(Range&& range, Index&& i) noexcept
{
  // check that the index is within the range
  SEN_EXPECT(static_cast<ptrdiff_t>(i) < (std::end(range) - std::begin(range)));

  // go to the beginning of the range and fetch the element at index i
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return std::begin(std::forward<Range>(range))[std::forward<Index>(i)];
}

/// Gets the element at index i in the range rng and checks the bounds (const version)
template <typename T, typename Range, typename Index>
inline const T& cindex(Range&& range, Index&& i) noexcept
{
  // check that the index is within the range
  SEN_EXPECT(static_cast<ptrdiff_t>(i) < (std::end(range) - std::begin(range)));

  // go to the beginning of the range and fetch the element at index i
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return std::begin(std::forward<Range>(range))[std::forward<Index>(i)];
}

}  // namespace impl

/// \addtogroup util
/// @{

/// Things that can go wrong when using a StaticVector.
enum class StaticVectorError : std::uint8_t
{
  full,      ///< The vector cannot hold more elements.
  badRange,  ///< The range is invalid.
  empty,     ///< Invalid operation on an empty vector.
};

/// Base class for stack-based, exception-safe and resizable vector with fixed-capacity.
///
/// Mimics the interface and general behavior of std::vector,
/// but it does not perform memory allocations and does not make
/// use of exceptions.
///
/// The idea is for it to be compatible with std algorithms and serve
/// as a replacement for std::vector in constrained or real-time environments.
///
/// Use StaticVectorBase<T> when using vectors but not depending on their capacity.
///
/// @note This class is not thread-aware.
///
/// @tparam T value type.
template <typename T>
class StaticVectorBase
{
public:  // checks
  static constexpr bool nothrowDes = std::is_nothrow_destructible_v<T>;
  static constexpr bool nothrowDefaultCons = std::is_nothrow_default_constructible_v<T>;
  static constexpr bool nothrowCopyCons = std::is_nothrow_copy_constructible_v<T>;
  static constexpr bool nothrowMoveCons = std::is_nothrow_move_constructible_v<T>;
  static constexpr bool nothrowCopyAndDes = nothrowDes && nothrowCopyCons;
  static constexpr bool nothrowDefaultConsAndDes = nothrowDefaultCons && nothrowDes;

  // the following types allow compatibility with stdlib algorithms
public:
  using value_type = T;                                                  // NOLINT(readability-identifier-naming)
  using difference_type = ptrdiff_t;                                     // NOLINT(readability-identifier-naming)
  using pointer = T*;                                                    // NOLINT(readability-identifier-naming)
  using const_pointer = T const*;                                        // NOLINT(readability-identifier-naming)
  using reference = T&;                                                  // NOLINT(readability-identifier-naming)
  using const_reference = T const&;                                      // NOLINT(readability-identifier-naming)
  using rvalue_reference = T&&;                                          // NOLINT(readability-identifier-naming)
  using iterator = pointer;                                              // NOLINT(readability-identifier-naming)
  using const_iterator = const_pointer;                                  // NOLINT(readability-identifier-naming)
  using size_type = std::size_t;                                         // NOLINT(readability-identifier-naming)
  using reverse_iterator = std::reverse_iterator<iterator>;              // NOLINT(readability-identifier-naming)
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;  // NOLINT(readability-identifier-naming)

public:
  using Maybe = Result<void, StaticVectorError>;
  using MaybeIterator = Result<iterator, StaticVectorError>;

public:  // special members
  StaticVectorBase(const StaticVectorBase&) noexcept = delete;
  StaticVectorBase(StaticVectorBase&&) noexcept = delete;
  StaticVectorBase& operator=(StaticVectorBase&&) noexcept = delete;
  StaticVectorBase& operator=(const StaticVectorBase&) noexcept = delete;

public:  // getters.
  /// Element count.
  /// Complexity: constant
  [[nodiscard]] size_type size() const noexcept { return size_; }

  /// Maximum number of elements that can be allocated in the storage.
  /// Complexity: constant
  [[nodiscard]] size_type capacity() const noexcept { return capacity_; }

  /// Max element count (same as capacity()).
  /// Complexity: constant
  [[nodiscard]] size_type maxSize() const noexcept { return capacity(); }

  /// Direct access to the underlying storage.
  /// Complexity: constant
  [[nodiscard]] const_pointer data() const noexcept
  {
    return reinterpret_cast<const_pointer>(dataPtr_);  // NOLINT NOSONAR
  }

  /// Direct access to the underlying storage.
  /// Complexity: constant
  [[nodiscard]] pointer data() noexcept
  {
    return reinterpret_cast<pointer>(dataPtr_);  // NOLINT NOSONAR
  }

  /// true if the container is empty.
  /// Complexity: constant
  [[nodiscard]] bool empty() const noexcept { return size() == size_type {0U}; }

  /// true if the container is full.
  /// Complexity: constant
  [[nodiscard]] bool full() const noexcept { return size() == capacity(); }

public:  // assignment
  /// Clears the vector and assigns n copies of u to it.
  /// Complexity: linear in n
  /// @post object contains n values of u.
  ///
  /// @param n the number of elements to assign.
  /// @param u the value to be copied in.
  /// @return Ok   - operation was successful
  ///         full - not enough capacity to hold n elements.
  [[nodiscard]] Maybe assign(size_type n, const T& u) noexcept(nothrowCopyCons);

  /// Initializer list assignment.
  /// Complexity: linear in list.size()
  /// @post object contains all the elements of list.
  ///
  /// @param list elements to be stored in the vector.
  /// @return Ok   - operation was successful
  ///         full - not enough capacity to hold list.size() elements.
  [[nodiscard]] Maybe assign(const std::initializer_list<T>& list) noexcept(nothrowCopyCons);

  /// Initializer list assignment (r-value version).
  /// Complexity: linear in list.size()
  /// @post object contains all the elements of list.
  ///
  /// @param list elements to be stored in the vector.
  /// @return Ok   - operation was successful
  ///         full - not enough capacity to hold list.size() elements.
  [[nodiscard]] Maybe assign(std::initializer_list<T>&& list) noexcept(nothrowMoveCons);

  /// Clears the vector and assigns a range to it.
  /// Complexity: linear in distance(first, last)
  /// @post object contains a copy of all the elements between first and last.
  ///
  /// @return Ok        - operation was successful
  ///         badRange - [first, last] is not a valid range.
  ///         full      - not enough capacity to hold list.size() elements.
  template <class InputIt>
  [[nodiscard]] Maybe assign(InputIt first, InputIt last) noexcept(nothrowCopyCons);

public:  // positional iterator accessors
  /// Returns an iterator to the first element of the vector.
  /// If the vector is empty, the returned iterator will be equal to end().
  /// Complexity: constant.
  [[nodiscard]] iterator begin() noexcept { return data(); }

  /// Returns an iterator to the first element of the vector.
  /// If the vector is empty, the returned iterator will be equal to end().
  /// Complexity: constant.
  [[nodiscard]] const_iterator begin() const noexcept { return data(); }

  /// Iterator to the element following the last element.
  /// Complexity: constant.
  [[nodiscard]] iterator end() noexcept
  {
    return data() + size();  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

  /// Iterator to the element following the last element.
  /// Complexity: constant.
  /// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  [[nodiscard]] const_iterator end() const noexcept { return data() + size(); }

  /// Reverse iterator to the first element.
  /// Complexity: constant.
  [[nodiscard]] reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  /// Reverse iterator to the first element.
  /// Complexity: constant.
  [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  /// Returns a reverse iterator to the element following the last element of the reversed vector.
  /// It corresponds to the element preceding the first element of the non-reversed vector.
  /// This element acts as a placeholder, attempting to access it results in undefined behavior.
  /// Complexity: constant.
  [[nodiscard]] reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  /// Returns a reverse iterator to the element following the last element of the reversed vector.
  /// It corresponds to the element preceding the first element of the non-reversed vector.
  /// This element acts as a placeholder, attempting to access it results in undefined behavior.
  /// Complexity: constant.
  [[nodiscard]] const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  /// Returns a const iterator to the first element of the vector.
  /// If the vector is empty, the returned iterator will be equal to cend().
  /// Complexity: constant.
  [[nodiscard]] const_iterator cbegin() noexcept { return begin(); }

  /// Returns a const iterator to the first element of the vector.
  /// If the vector is empty, the returned iterator will be equal to cend().
  /// Complexity: constant.
  [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }

  /// Constant iterator to the element following the last element.
  /// Complexity: constant.
  [[nodiscard]] const_iterator cend() noexcept { return end(); }

  /// Constant iterator to the element following the last element.
  /// Complexity: constant.
  [[nodiscard]] const_iterator cend() const noexcept { return end(); }

public:  // index-based value accessors
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
  [[nodiscard]] reference operator[](size_type i) noexcept { return impl::index<value_type>(*this, i); }

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
  [[nodiscard]] const_reference operator[](size_type i) const noexcept { return impl::cindex<value_type>(*this, i); }

public:  // position-based value accessors
  /// The element at the beginning of the container.
  /// Calling front on an empty container is undefined.
  /// Complexity: constant
  /// @pre !empty()
  [[nodiscard]] reference front() noexcept
  {
    SEN_EXPECT(!empty());
    return impl::index<value_type>(*this, 0U);
  }

  /// The element at the beginning of the container (const version).
  /// Calling front on an empty container is undefined.
  /// Complexity: constant
  /// @pre !empty()
  [[nodiscard]] const_reference front() const noexcept
  {
    SEN_EXPECT(!empty());
    return impl::cindex<value_type>(*this, 0U);
  }

  /// The element at the end of the container.
  /// Calling back on an empty container is undefined.
  /// Complexity: constant
  /// @pre !empty()
  [[nodiscard]] reference back() noexcept
  {
    SEN_EXPECT(!empty());
    return impl::index<value_type>(*this, size() - 1);
  }

  /// The element at the end of the container (const version).
  /// Calling back on an empty container is undefined.
  /// Complexity: constant
  /// @pre !empty()
  [[nodiscard]] const_reference back() const noexcept
  {
    SEN_EXPECT(!empty());
    return impl::cindex<value_type>(*this, size() - 1);
  }

public:  // modifiers
  /// Clears the vector.
  /// This function destroys all the elements.
  /// Complexity: linear in size()
  /// @post size() == 0
  /// @post empty()
  void clear() noexcept(nothrowDes)
  {
    internalDestroyAll();
    internalSetSize(0U);
  }

  /// Appends a value to the end of the container (r-value version).
  /// Complexity: constant.
  /// @return Ok   - operation was successful
  ///         full - not enough capacity in the vector.
  ///
  /// NOLINTNEXTLINE(readability-identifier-naming)
  Maybe push_back(T&& value) noexcept(nothrowMoveCons);

  /// Appends a value to the end of the container.
  /// Complexity: constant.
  /// @return Ok   - operation was successful
  ///         full - not enough capacity in the vector.
  ///
  /// NOLINTNEXTLINE(readability-identifier-naming)
  Maybe push_back(const T& value) noexcept(nothrowCopyCons);

  /// Appends a value (default constructed) to the end of the container.
  /// Complexity: constant.
  /// @return Ok   - operation was successful
  ///         full - not enough capacity in the vector.
  ///
  /// NOLINTNEXTLINE(readability-identifier-naming)
  Maybe push_back() noexcept(nothrowDefaultCons);

  /// Inserts a new element into the container directly before pos.
  /// The arguments args... are forwarded to the constructor as std::forward<Args>(args)
  /// args... may directly or indirectly refer to a value in the container.
  ///
  /// Only the iterators and references before the insertion point remain valid. The past-the-end
  /// iterator is also invalidated.
  ///
  /// Complexity: linear in size().
  ///
  /// @return badRange - position was not in a valid range
  ///         full      - not enough capacity in the vector.
  ///         iterator  - pointing to the emplaced element.
  template <typename... Args>
  MaybeIterator emplace(iterator position, Args&&... args) noexcept(nothrowMoveCons);

  /// Constructs an element in-place at the end of the embedded storage.
  ///
  /// Appends a new element to the end of the container.
  /// The arguments args... are forwarded to the constructor as std::forward<Args>(args)
  ///
  /// Only the past-the-end iterator is invalidated.
  ///
  /// Complexity: constant
  ///
  /// @tparam Args      - arguments for constructing the element.
  /// @return full      - not enough capacity in the vector.
  ///         iterator  - pointing to the emplaced element.
  template <typename... Args>
  Maybe emplace_back(Args&&... args) noexcept(  //  NOLINT(readability-identifier-naming)
    nothrowMoveCons);

  /// Removes the last element of the container.
  /// Iterators and references to the last element, as well as the end() iterator, are invalidated.
  /// Complexity: constant
  /// @return empty  - attempted to pop_back on an empty vector.
  ///         ok     - successful operation.
  Maybe pop_back() noexcept(nothrowDes);  //  NOLINT(readability-identifier-naming)

public:  // insert overloads
  /// Inserts and element before position.
  /// Complexity: linear in size()
  ///
  /// @param position   - where to insert the new element.
  /// @param x          - element to insert.
  /// @return badRange - position was not in a valid range.
  ///         full      - not enough capacity in the vector.
  ///         iterator  - pointing to the inserted element.
  MaybeIterator insert(iterator position, const_reference x) noexcept(nothrowCopyCons);

  /// Inserts and element before position (r-value version).
  /// Complexity: linear in size()
  ///
  /// @param position   - where to insert the new element
  /// @param x          - element to insert
  /// @return badRange - position was not in a valid range.
  ///         full      - not enough capacity in the vector.
  ///         iterator  - pointing to the inserted element.
  MaybeIterator insert(iterator position, value_type&& x) noexcept(nothrowMoveCons);

  /// Inserts n copies of the value before pos
  /// Complexity: linear in size() and n
  ///
  /// @param position   - where to insert the new elements.
  /// @param n          - number of elements to insert.
  /// @param x          - value of the elements to insert.
  /// @return badRange - position was not in a valid range.
  ///         full      - not enough capacity in the vector.
  ///         iterator  - iterator pointing to the first element inserted, or position if n == 0.
  MaybeIterator insert(iterator position, size_type n, const T& x) noexcept(nothrowCopyCons);

  /// Inserts a range of values via initializer list.
  /// Complexity: linear in size() and list.size()
  ///
  /// @param position   - where to insert the new elements.
  /// @param list       - elements to insert.
  /// @return badRange - position was not in a valid range.
  ///         full      - not enough capacity in the vector.
  ///         iterator  - iterator pointing to the first element inserted, or position if
  ///         list.size() == 0.
  MaybeIterator insert(iterator position, std::initializer_list<T> list) noexcept(nothrowCopyCons);

  /// Inserts elements from range [first, last) before position.
  /// Complexity: linear in size() and distance(first, last)
  ///
  /// @tparam InputIt   - type of the input iterators.
  /// @param position   - where to insert the new elements.
  /// @param first      - range start.
  /// @param last       - range end.
  /// @return badRange - [first, last) is not a valid range.
  ///         full      - not enough capacity in the vector.
  ///         iterator  - iterator pointing to the first element inserted, or position if first ==
  ///         last.
  template <class InputIt>
  MaybeIterator insert(iterator position, InputIt first, InputIt last) noexcept(nothrowCopyCons);

  /// Inserts elements from range [first, last) before position (by moving them).
  /// Complexity: linear in size() and distance(first, last)
  ///
  /// @tparam InputIt   - type of the input iterators.
  /// @param position   - where to insert the new elements.
  /// @param first      - range start.
  /// @param last       - range end.
  /// @return badRange - [first, last) is not a valid range.
  ///         full      - not enough capacity in the vector.
  ///         iterator  - iterator pointing to the first element inserted, or position if first ==
  ///         last.
  template <class InputIt>
  MaybeIterator move_insert(iterator position,  //  NOLINT(readability-identifier-naming)
                            InputIt first,
                            InputIt last) noexcept(nothrowMoveCons);

public:  // resizing
  /// Changes the size of the container to newSize.
  /// If need to be appended, elements are copy-constructed from value.
  /// Complexity: linear in newSize
  ///
  /// @param newSize - expected new size of the vector.
  /// @param value    - to copy the new elements
  /// @return full    - not enough capacity in the vector.
  ///         ok      - successful operation.
  Maybe resize(size_type newSize, const T& value) noexcept(nothrowCopyAndDes);

  /// Changes the size of  the container to newSize.
  /// If need to be appended, elements are move-constructed from T{}.
  /// Complexity: linear in newSize
  ///
  /// @param newSize - expected new size of the vector.
  /// @return full    - not enough capacity in the vector.
  ///         ok      - successful operation.
  Maybe resize(size_type newSize) noexcept(nothrowDefaultConsAndDes);

public:  // erase
  /// Remove an element at position from the container.
  /// Complexity: linear in size()
  ///
  /// Invalidates iterators and references at or after the point of the erase, including the end()
  /// iterator.
  ////
  /// The iterator position must be valid and dereferenceable. Thus the end() iterator (which is
  /// valid, but is not dereferenceable) cannot be used as a value for position.
  ///
  /// @param position - where to remove the element.
  /// @return badRange - position is not in a valid range.
  ///         iterator  - Iterator following the last removed element.
  ///                     If position refers to the last element, then the end() iterator is
  ///                     returned.
  MaybeIterator erase(iterator position) noexcept(nothrowDes);

  /// Remove a range of elements from the container.
  /// Complexity: linear in size() and distance(first, last)
  ///
  /// Invalidates iterators and references at or after the point of the erase, including the end()
  /// iterator. The iterator first does not need to be dereferenceable if first==last: erasing an
  /// empty range is a no-op.
  ///
  /// @param first
  /// @param last
  /// @return badRange - first, last is not a valid range.
  ///         iterator  - iterator following the last removed element.
  ///                     if [first, last) is an empty range, then last is returned.
  MaybeIterator erase(iterator first, iterator last) noexcept(nothrowDes);

private:  // implementation details
  /// Append n elements to the end of the vector
  /// Complexity: linear in n
  [[nodiscard]] Maybe emplaceMultiple(size_type n) noexcept(nothrowDefaultCons);

  /// Asserts that the iterator is within the range of the container.
  template <typename Iterator>
  [[nodiscard]] Maybe checkIteratorRange(Iterator itr) noexcept;

  /// Asserts that a pair of iterators are valid with respect to each-other.
  template <typename IteratorA, typename IteratorB>
  [[nodiscard]] Maybe checkIteratorPair(IteratorA first, IteratorB last) noexcept;

  /// Destroy elements in the range [begin, end).
  ///
  /// @note: The size of the storage is not changed.
  /// Complexity: linear in distance(first, last)
  template <class Itr>
  void internalDestroy(Itr first, Itr last) noexcept(nothrowDes);

  /// Destroys all elements of the storage.
  /// @note: The size of the storage is not changed.
  /// Complexity: linear in size()
  void internalDestroyAll() noexcept(nothrowDes) { internalDestroy(data(), end()); }

protected:
  StaticVectorBase(std::size_t capacity, void* dataPtr) noexcept: capacity_(capacity), dataPtr_(dataPtr)
  {
    SEN_EXPECT(dataPtr != nullptr);
  }
  virtual ~StaticVectorBase() noexcept { internalDestroyAll(); }

  /// Changes the container size to newSize.
  /// @pre: `newSize <= capacity()`.
  /// @note No elements are constructed or destroyed.
  /// Complexity: constant
  void internalSetSize(std::size_t newSize) noexcept;

private:
  std::size_t capacity_;
  void* dataPtr_;
  std::size_t size_ = 0U;
};

/// Stack-based, exception-free and resizable vector with fixed-capacity.
///
/// Mimics the interface and general behavior of std::vector,
/// but it does not perform memory allocations and does not make
/// use of exceptions.
///
/// The idea is for it to be compatible with std algorithms and serve
/// as a replacement for std::vector in constrained environments.
///
/// @note This class is not thread-aware.
///
/// @tparam T value type.
/// @tparam s maximum size of the vector (capacity).
template <typename T, std::size_t s>
class StaticVector: public StaticVectorBase<T>
{
public:
  static constexpr std::size_t staticCapacity = s;
  static_assert(staticCapacity != 0U, "Vectors of no capacity are not supported");

public:
  using Base = StaticVectorBase<T>;

public:
  /// Creates an empty vector.
  StaticVector() noexcept: StaticVectorBase<T>(s, &data_) {}

  /// Create a vector with n default-constructed elements.
  /// Complexity: linear in n.
  explicit StaticVector(std::size_t n) noexcept;

  /// Create a vector with n copies of value.
  /// Complexity: linear in n.
  StaticVector(std::size_t n, T const& value) noexcept(Base::nothrowCopyCons);

  /// Create a vector from initializer list.
  /// Complexity: linear in list.size().
  StaticVector(std::initializer_list<T> list) noexcept(Base::nothrowCopyCons);

  /// Create a vector from range [first, last).
  /// Complexity: linear in distance(first, last).
  template <class InputIt>
  StaticVector(InputIt first, InputIt last) noexcept(Base::nothrowCopyCons);

  /// Copy constructor.
  /// Complexity: linear in other.size().
  StaticVector(const StaticVector& other) noexcept(Base::nothrowCopyCons);

  /// Move constructor.
  /// @post After the move, other is guaranteed to be empty().
  /// Complexity: linear in other.size()
  StaticVector(StaticVector&& other) noexcept;

  /// Deletes all internal elements.
  /// Complexity: linear in size().
  ~StaticVector() noexcept override = default;

  /// Copy assignment.
  /// Complexity: linear in other.size().
  StaticVector& operator=(const StaticVector& other) noexcept(Base::nothrowCopyCons);

  /// Move assignment.
  /// @post After the move, other is guaranteed to be empty().
  /// Complexity: linear in other.size().
  StaticVector& operator=(StaticVector&& other) noexcept;

  /// Swaps the content of this vector with other.
  /// Complexity: linear in size() and x.size().
  void swap(StaticVector& other) noexcept;

  /// This object, as its base class.
  [[nodiscard]] Base& base() noexcept { return *this; }

  /// This object, as its base class.
  [[nodiscard]] const Base& base() const noexcept { return *this; }

private:
  alignas(T) std::byte data_[sizeof(T) * s];
};

//--------------------------------------------------------------------------------------------------------------
// Comparison operators
//--------------------------------------------------------------------------------------------------------------

/// Checks if the contents of lhs and rhs are equal, that is, they have the same number of elements
/// and each element in lhs compares equal with the element in rhs at the same position.
template <typename T, std::size_t size>
std::enable_if_t<HasOperator<T>::eq, bool> operator==(StaticVector<T, size> const& lhs,
                                                      StaticVector<T, size> const& rhs) noexcept
{
  if (lhs.size() != rhs.size())
  {
    return false;
  }

  return lhs.size() == 0 || std::equal(lhs.begin(), lhs.end(), rhs.begin(), std::equal_to<T> {});
}

/// Equivalent to negating operator ==.
template <typename T, std::size_t size>
std::enable_if_t<HasOperator<T>::ne, bool> operator!=(StaticVector<T, size> const& lhs,
                                                      StaticVector<T, size> const& rhs) noexcept
{
  return !(lhs == rhs);
}

/// @}

}  // namespace sen

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_VECTOR_TRY(...)                                                                                            \
  {                                                                                                                    \
    auto res = __VA_ARGS__;                                                                                            \
    if (res.isError())                                                                                                 \
    {                                                                                                                  \
      return Err(res.getError());                                                                                      \
    }                                                                                                                  \
  }

namespace sen
{

template <typename T>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::assign(StaticVectorBase::size_type n,
                                                                       const T& u) noexcept(nothrowCopyCons)
{
  if (n > capacity())
  {
    return Err(StaticVectorError::full);
  }

  // clear and insert the elements at the beginning
  clear();
  return insert(begin(), n, u);
}

template <typename T>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::assign(const std::initializer_list<T>& list) noexcept(
  nothrowCopyCons)
{
  if (list.size() > capacity())
  {
    return Err(StaticVectorError::full);
  }

  clear();
  return insert(begin(), list.begin(), list.end());
}

template <typename T>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::assign(std::initializer_list<T>&& list) noexcept(
  nothrowMoveCons)
{
  if (list.size() > capacity())
  {
    return Err(StaticVectorError::full);
  }

  // clear and move-insert the elements at the beginning
  clear();
  return move_insert(begin(), list.begin(), list.end());
}

template <typename T>
template <class InputIt>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::assign(InputIt first,
                                                                       InputIt last) noexcept(nothrowCopyCons)
{
  // check capacity
  if (last - first < 0)
  {
    return Err(StaticVectorError::badRange);
  }

  if (static_cast<size_type>(last - first) > capacity())
  {
    return Err(StaticVectorError::full);
  }

  // clear and insert the elements at the beginning
  clear();
  return insert(begin(), first, last);
}

template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::push_back(T&& value) noexcept(nothrowMoveCons)
{
  if (full())
  {
    return Err(StaticVectorError::full);
  }
  return emplace_back(std::move(value));
}

template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::push_back(const T& value) noexcept(nothrowCopyCons)
{
  if (full())
  {
    return Err(StaticVectorError::full);
  }

  return emplace_back(value);
}

template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::push_back() noexcept(nothrowDefaultCons)
{
  static_assert(std::is_default_constructible<T>::value,
                "this version of push_back requires a default constructible type");

  if (full())
  {
    return Err(StaticVectorError::full);
  }
  return emplace_back(T {});
}

template <typename T>
template <typename... Args>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::emplace(
  StaticVectorBase::iterator position,
  Args&&... args) noexcept(nothrowMoveCons)
{
  if (full())
  {
    return Err(StaticVectorError::full);
  }

  // check that the position is valid
  SEN_VECTOR_TRY(checkIteratorRange(position))

  if (position == end())
  {
    std::ignore = emplace_back(std::forward<Args...>(args...));  // not full
    return Ok(position);
  }

  const auto previousEnd = end();

  // move the back to a new extra place
  std::ignore = emplace_back(std::move(back()));  // not full

  if (std::distance(position, previousEnd) != 1)
  {
    // move the rest of the elements to make room
    std::move_backward(position, end() - 2, end() - 1);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    // destroy the element that was in position
    std::destroy_at(position);
  }

  // place the new element in position
  ::new (std::addressof(*position)) T(std::forward<Args>(args)...);

  return Ok(position);
}

template <typename T>
template <typename... Args>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::emplace_back(Args&&... args) noexcept(nothrowMoveCons)
{
  if (full())
  {
    return Err(StaticVectorError::full);
  }

  // placement new at the end of the array
  new (end()) T(std::forward<Args>(args)...);

  // increase the size
  internalSetSize(size() + 1);
  return Ok();
}

template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::pop_back() noexcept(nothrowDes)
{
  if (empty())
  {
    return Err(StaticVectorError::empty);
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  auto ptr = end() - 1;         // get the pointer to the last element
  std::destroy_at(ptr);         // do placement delete
  internalSetSize(size() - 1);  // decrease the size
  return Ok();
}

template <typename T>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::insert(
  StaticVectorBase::iterator position,
  const_reference x) noexcept(nothrowCopyCons)
{
  if (full())
  {
    return Err(StaticVectorError::full);
  }

  SEN_VECTOR_TRY(checkIteratorRange(position))
  return insert(position, static_cast<size_type>(1), x);
}

template <typename T>
template <class InputIt>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::insert(iterator position,
                                                                               InputIt first,
                                                                               InputIt last) noexcept(nothrowCopyCons)
{
  // checks
  SEN_VECTOR_TRY(checkIteratorRange(position))
  SEN_VECTOR_TRY(checkIteratorPair(first, last))

  // check if we would be inserting beyond the capacity
  if (size() + static_cast<size_type>(last - first) > capacity())
  {
    return Err(StaticVectorError::full);
  }

  const size_t insertN = std::distance(first, last);
  const size_t insertBegin = std::distance(begin(), position);
  const size_t insertEnd = insertBegin + insertN;

  // move old data.
  size_t copyOldN;
  size_t constructOldN;
  iterator pConstructOld;

  auto pEnd = end();

  if (insertEnd > size())
  {
    copyOldN = 0;
    constructOldN = size() - insertBegin;
    pConstructOld = begin() + insertEnd;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
  else
  {
    copyOldN = size() - insertBegin - insertN;
    constructOldN = insertN;
    pConstructOld = pEnd;
  }

  const size_t copyNewN = constructOldN;
  const size_t constructNewN = insertN - copyNewN;

  // move-construct old on the new (uninitialized memory area).
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::uninitialized_move(pEnd - constructOldN, pEnd, pConstructOld);

  // move old to the remaining (already initialized) memory area.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::move_backward(begin() + insertBegin, begin() + insertBegin + copyOldN, begin() + insertEnd + copyOldN);

  // copy-construct new in the (uninitialized) memory area
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::uninitialized_copy(first + copyNewN, first + copyNewN + constructNewN, pEnd);

  // copy new to the final location
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::copy(first, first + copyNewN, begin() + insertBegin);

  internalSetSize(size_ + insertN);

  return Ok(position);
}

template <typename T>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::insert(iterator position,
                                                                               value_type&& x) noexcept(nothrowMoveCons)
{
  if (full())
  {
    return Err(StaticVectorError::full);
  }

  SEN_VECTOR_TRY(checkIteratorRange(position))

  if (position == end())
  {
    std::ignore = emplace_back(std::move(x));  // not full, so cannot fail
  }
  else
  {
    // move the back to a new extra place
    std::ignore = emplace_back(std::move(back()));  // not full, so cannot fail

    // move affected range one position
    std::move_backward(position, end() - 2, end() - 1);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    // move our value into the desired position
    *position = std::move(x);
  }

  return Ok(position);
}

template <typename T>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::insert(StaticVectorBase::iterator position,
                                                                               StaticVectorBase::size_type n,
                                                                               const T& x) noexcept(nothrowCopyCons)
{
  SEN_VECTOR_TRY(checkIteratorRange(position))

  if (const auto newSize = size() + n; newSize > capacity())
  {
    return Err(StaticVectorError::full);
  }

  if (n == 0U)
  {
    return Ok(position);
  }

  const size_t insertN = n;
  const size_t insertBegin = std::distance(begin(), position);
  const size_t insertEnd = insertBegin + insertN;

  // copy old data.
  size_t copyOldN;
  size_t constructOldN;
  iterator pConstructOld;

  auto pEnd = end();

  if (insertEnd > size())
  {
    copyOldN = 0;
    constructOldN = size() - insertBegin;
    pConstructOld = begin() + insertEnd;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
  else
  {
    copyOldN = size() - insertBegin - insertN;
    constructOldN = insertN;
    pConstructOld = pEnd;
  }

  size_t copyNewN = constructOldN;
  size_t constructNewN = insertN - copyNewN;

  // move-construct old on the new (uninitialized memory area).
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::uninitialized_move(static_cast<iterator>(pEnd - constructOldN), pEnd, pConstructOld);

  // move old to the remaining (already initialized) memory area.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::move_backward(begin() + insertBegin, begin() + insertBegin + copyOldN, begin() + insertEnd + copyOldN);

  // construct new in the (uninitialized) memory area.
  std::uninitialized_fill_n(pEnd, constructNewN, x);

  // copy new n values of T
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::fill_n(begin() + insertBegin, copyNewN, x);

  internalSetSize(size() + n);

  return Ok(position);
}

template <typename T>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::insert(
  StaticVectorBase::iterator position,
  std::initializer_list<T> list) noexcept(nothrowCopyCons)
{
  SEN_VECTOR_TRY(checkIteratorRange(position))
  return insert(position, list.begin(), list.end());
}

template <typename T>
template <class InputIt>
inline typename StaticVectorBase<T>::MaybeIterator
StaticVectorBase<T>::move_insert(iterator position, InputIt first, InputIt last) noexcept(nothrowMoveCons)
{
  // checks
  SEN_VECTOR_TRY(checkIteratorRange(position))
  SEN_VECTOR_TRY(checkIteratorPair(first, last))

  if (size() + static_cast<size_type>(last - first) > capacity())
  {
    return Err(StaticVectorError::full);
  }

  const size_t insertN = std::distance(first, last);
  const size_t insertBegin = std::distance(begin(), position);
  const size_t insertEnd = insertBegin + insertN;

  // move old data.
  size_t moveOldN;
  size_t constructOldN;
  iterator pConstructOld;
  auto pEnd = end();

  if (insertEnd > size())
  {
    moveOldN = 0;
    constructOldN = size() - insertBegin;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    pConstructOld = begin() + insertEnd;   // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }
  else
  {
    moveOldN = size() - insertBegin - insertN;
    constructOldN = insertN;
    pConstructOld = pEnd;
  }

  const size_t copyNewN = constructOldN;
  const size_t constructNewN = insertN - copyNewN;

  // move-construct old on the new (uninitialized memory area).
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::uninitialized_move(pEnd - constructOldN, pEnd, pConstructOld);

  // move old to the remaining (already initialized) memory area.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::move_backward(begin() + insertBegin, begin() + insertBegin + moveOldN, begin() + insertEnd + moveOldN);

  // move-construct new in the (uninitialized) memory area
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::uninitialized_move(first + copyNewN, first + copyNewN + constructNewN, pEnd);

  // move new to the final location
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::move(first, first + copyNewN, begin() + insertBegin);

  internalSetSize(size_ + insertN);

  return Ok(position);
}

template <typename T>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::resize(StaticVectorBase::size_type newSize,
                                                                       const T& value) noexcept(nothrowCopyAndDes)
{
  static_assert(std::is_nothrow_copy_constructible<T>::value, "T is not supported");

  // do nothing if the size is already the same
  if (newSize == size())
  {
    return Ok();
  }

  if (newSize > capacity())
  {
    return Err(StaticVectorError::full);
  }

  if (newSize > size())
  {
    // insert copies of value at the end
    return insert(end(), newSize - size(), value);
  }

  // remove elements from the end
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return erase(end() - (size() - newSize), end());
}

template <typename T>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::resize(StaticVectorBase::size_type newSize) noexcept(
  nothrowDefaultConsAndDes)
{
  // do nothing if the size is already the same
  if (newSize == size())
  {
    return Ok();
  }

  if (newSize > capacity())
  {
    return Err(StaticVectorError::full);
  }

  if (newSize > size())
  {
    return emplaceMultiple(newSize - size());
  }

  // remove elements from the end
  return erase(end() - (size() - newSize), end());  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

template <typename T>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::erase(
  StaticVectorBase::iterator position) noexcept(nothrowDes)
{
  SEN_VECTOR_TRY(checkIteratorRange(position))

  // move elements from [position + 1, end) to position
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::move(position + 1, end(), position);

  // destroy the last element
  auto ptr = end() - 1;  // get the pointer to the last element NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::destroy_at(ptr);  // do placement delete
  internalSetSize(size() - 1);
  return Ok(position);
}

template <typename T>
inline typename StaticVectorBase<T>::MaybeIterator StaticVectorBase<T>::erase(
  StaticVectorBase::iterator first,
  StaticVectorBase::iterator last) noexcept(nothrowDes)
{
  if (first == end())  // early exit
  {
    return Err(StaticVectorError::badRange);
  }

  if (first == begin() && last == end())
  {
    clear();
  }
  else
  {
    SEN_VECTOR_TRY(checkIteratorPair(first, last))
    SEN_VECTOR_TRY(checkIteratorRange(first))
    SEN_VECTOR_TRY(checkIteratorRange(last))

    // move the remaining elements on top of the erased range
    std::move(last, end(), first);

    // destroy the elements left over at the end.
    const auto nDelete = std::distance(first, last);
    std::destroy(end() - nDelete, end());  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    internalSetSize(size() - nDelete);
  }

  return Ok(first);
}

template <typename T>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::emplaceMultiple(StaticVectorBase::size_type n) noexcept(
  nothrowDefaultCons)
{
  const size_type newSize = size() + n;
  SEN_EXPECT(newSize <= capacity());

  while (newSize != size())
  {
    // cannot overflow as newSize != size() && newSize <= capacity
    std::ignore = emplace_back();
  }

  return Ok();
}

template <typename T>
template <typename Iterator>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::checkIteratorRange(Iterator itr) noexcept
{
  if (itr >= begin() && itr <= end())
  {
    return Ok();
  }

  return Err(StaticVectorError::badRange);
}

template <typename T>
template <typename IteratorA, typename IteratorB>
inline typename StaticVectorBase<T>::Maybe StaticVectorBase<T>::checkIteratorPair(IteratorA first,
                                                                                  IteratorB last) noexcept
{
  if (first > last)
  {
    return Err(StaticVectorError::badRange);
  }
  return Ok();
}

template <typename T>
inline void StaticVectorBase<T>::internalSetSize(std::size_t newSize) noexcept
{
  SEN_EXPECT(newSize <= capacity());  // newSize out-of-bounds [0, Capacity)
  size_ = newSize;
}

template <typename T>
template <class InputIt>
inline void StaticVectorBase<T>::internalDestroy(InputIt first, InputIt last) noexcept(nothrowDes)
{
  SEN_EXPECT(first >= data());
  SEN_EXPECT(first <= end());
  SEN_EXPECT(last >= data());
  SEN_EXPECT(last <= end());

  for (; first != last; ++first)  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  {
    std::destroy_at(first);
  }
}

// StaticVector

template <typename T, std::size_t c>
inline StaticVector<T, c>::StaticVector(std::size_t n) noexcept: StaticVectorBase<T>(c, &data_)
{
  auto result = this->resize(n);
  SEN_ASSERT(result.isOk());
  std::ignore = result;
}

template <typename T, std::size_t c>
inline StaticVector<T, c>::StaticVector(std::size_t n, const T& value) noexcept(Base::nothrowCopyCons)
  : StaticVectorBase<T>(c, &data_)
{
  auto result = this->resize(n, value);
  SEN_ASSERT(result.isOk());
  std::ignore = result;
}

template <typename T, std::size_t c>
inline StaticVector<T, c>::StaticVector(std::initializer_list<T> list) noexcept(Base::nothrowCopyCons)
  : StaticVectorBase<T>(c, &data_)
{
  auto result = this->assign(std::move(list));
  SEN_ASSERT(result.isOk());
  std::ignore = result;
}

template <typename T, std::size_t c>
template <class InputIt>
inline StaticVector<T, c>::StaticVector(InputIt first, InputIt last) noexcept(Base::nothrowCopyCons)
  : StaticVectorBase<T>(c, &data_)
{
  auto result = this->assign(first, last);
  SEN_ASSERT(result.isOk());
  std::ignore = result;
}

template <typename T, std::size_t c>
inline StaticVector<T, c>::StaticVector(const StaticVector& other) noexcept(Base::nothrowCopyCons)
  : StaticVectorBase<T>(c, &data_)
{
  auto result = this->insert(this->begin(), other.begin(), other.end());
  SEN_ASSERT(result.isOk());
  std::ignore = result;
}

template <typename T, std::size_t c>
inline StaticVector<T, c>::StaticVector(StaticVector&& other) noexcept: StaticVectorBase<T>(c, &data_)
{
  auto result = this->move_insert(this->begin(), other.begin(), other.end());
  SEN_ASSERT(result.isOk());
  std::ignore = result;
  other.internalSetSize(0U);
}

template <typename T, std::size_t c>
inline StaticVector<T, c>& StaticVector<T, c>::operator=(const StaticVector& other) noexcept(Base::nothrowCopyCons)
{
  if (this != &other)
  {
    // clear and insert the elements at the beginning
    this->clear();
    auto result = this->insert(this->begin(), other.begin(), other.end());
    SEN_ASSERT(result.isOk());
    std::ignore = result;
  }
  return *this;
}

template <typename T, std::size_t c>
inline StaticVector<T, c>& StaticVector<T, c>::operator=(StaticVector&& other) noexcept
{
  if (this != &other)
  {
    // clear and move-insert the elements at the beginning
    this->clear();
    auto result = this->move_insert(this->begin(), other.begin(), other.end());
    SEN_ASSERT(result.isOk());
    std::ignore = result;
    other.internalSetSize(0U);
  }
  return *this;
}

template <typename T, std::size_t c>
inline void StaticVector<T, c>::swap(StaticVector& other) noexcept
{
  StaticVector tmp = std::move(other);
  other = std::move(*this);
  (*this) = std::move(tmp);
}

}  // namespace sen

#undef SEN_VECTOR_TRY

#endif  // SEN_CORE_BASE_STATIC_VECTOR_H
