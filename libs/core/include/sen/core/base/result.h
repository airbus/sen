// === result.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_RESULT_H
#define SEN_CORE_BASE_RESULT_H

// sen
#include "sen/core/base/class_helpers.h"  // NOLINT(misc-include-cleaner)

// std
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

namespace sen
{

/// \addtogroup error_handling
/// @{

namespace impl
{

/// Holds a successful result.
template <typename T>
struct Ok final
{
  using ValueT = T;

  explicit Ok(T theVal) noexcept(std::is_nothrow_move_constructible_v<T>): val(std::move(theVal)) {}

  ~Ok() noexcept = default;
  SEN_COPY_CONSTRUCT(Ok) = default;  // NOLINT(misc-include-cleaner)
  SEN_MOVE_CONSTRUCT(Ok) = default;  // NOLINT(misc-include-cleaner)
  SEN_COPY_ASSIGN(Ok) = default;     // NOLINT(misc-include-cleaner)
  SEN_MOVE_ASSIGN(Ok) = default;     // NOLINT(misc-include-cleaner)

  T val;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Indicates a successful result with no payload.
template <>
struct Ok<void> final
{
};

/// Holds a non-successful result.
template <typename E>
struct Err final
{
  using ValueT = E;

  explicit Err(E theVal) noexcept(std::is_nothrow_move_constructible_v<E>): val(std::move(theVal)) {}

  ~Err() noexcept = default;
  SEN_COPY_CONSTRUCT(Err) = default;
  SEN_MOVE_CONSTRUCT(Err) = default;
  SEN_COPY_ASSIGN(Err) = default;
  SEN_MOVE_ASSIGN(Err) = default;

  E val;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Indicates a non-successful result with no payload.
template <>
struct Err<void> final
{
};

/// Convenience type for enable_if.
template <typename U>
using NonVoidT = std::enable_if_t<!std::is_void_v<U>, U>;

/// SEN_EXPECT in disguise, but since it depends on Result itself, we cannot include it here.
void resultExpect(bool value, std::string_view errorMsg = {}) noexcept;

}  // namespace impl

/// Result<T, E> is a template type that can be used to return and propagate errors.
/// The intent is to replace exceptions in this context. Result<T, E> is an algebraic
/// data type of Ok(T) that represents success and Err(E) representing an error.
///
/// Design of this class has been mainly inspired by Rust's std::result.
/// Example:
/// @code{.cpp}
///   enum class ErrorCode { too_small, too_large };
///
///   Result<int, ErrorCode> doSomething(int a) noexcept
///   {
///     if (a < 0)
///     {
///       return Err(ErrorCode::too_small);
///     }
///     return Ok(a + 100);
///   }
///
///   Result<void, ErrorCode> doSomethingElse(int a) noexcept
///   {
///     if (a < 0)
///     {
///       return Err(ErrorCode::too_small);
///     }
///     return Ok();
///   }
/// @endcode
///
/// Use isOk(), isError() or the bool() operator to check if there was an error.
/// Use getError() to get the error data (terminates the program if there was no error).
/// Use getValue() to determine the return value if there is no error (terminates the program
/// otherwise). Use getValueOr(X) as getValue() but returns X if there was an error.
///
/// Example:
/// @code{.cpp}
///   if (auto result = doSomething())       // result cannot be ignored (would not compile)
///   {
///     const auto& val = result.getValue(); // do something with val, if there is any
///   }                                      // (getValue is not present when T is 'void')
///   else
///   {
///     auto err = result.getError();        // do something with err
///   }
/// @endcode
///
/// @note This class is not thread-aware.
///
/// @tparam T the type of the result if there is no error (can be void).
/// @tparam E the type of the error.
template <typename T, typename E>
class [[nodiscard]] Result final
{
  static_assert(!::std::is_void_v<E>, "void error type is not allowed");

public:  // types
  using ValueType = T;
  using ErrorType = E;

public:  // construction
  /// Construct a Result that indicates success and that carries a valid return value.
  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  Result(impl::Ok<T>&& ok) noexcept: value_(std::in_place_type<T>, std::move(ok.val)) {}

  /// Construct a Result that indicates failure and that carries an error value.
  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  Result(impl::Err<E>&& err) noexcept: value_(std::in_place_type<E>, std::move(err.val)) {}

public:  // special members
  SEN_MOVE_CONSTRUCT(Result) = default;
  SEN_COPY_CONSTRUCT(Result) = default;
  SEN_MOVE_ASSIGN(Result) = default;
  SEN_COPY_ASSIGN(Result) = default;
  ~Result() = default;

public:  // conversion support
  /// Do a conversion construction from a compatible Result
  template <typename U,
            typename G,
            std::enable_if_t<std::conjunction_v<std::is_constructible<T, const U&>, std::is_constructible<E, const G&>>,
                             bool> = true>
  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  Result(const Result<U, G>& other)
    : value_(
        [](const Result<U, G>& other)
        {
          if (other.isOk())
          {
            return decltype(value_)(std::in_place_type<T>, other.getValue());
          }

          return decltype(value_)(std::in_place_type<E>, other.getError());
        }(other))
  {
  }

  /// Do a conversion construction from a compatible Result
  template <typename U,
            typename G,
            std::enable_if_t<std::conjunction_v<std::is_constructible<T, U>, std::is_constructible<E, G>>, bool> = true>
  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  Result(Result<U, G>&& other)
    : value_(
        [](Result<U, G>&& other)
        {
          if (other.isOk())
          {
            return decltype(value_)(std::in_place_type<T>, std::move(other).getValue());
          }

          return decltype(value_)(std::in_place_type<E>, std::move(other).getError());
        }(std::move(other)))
  {
  }

public:  // comparison
  bool operator==(const Result& other) const { return value_ == other.value_; }
  bool operator!=(const Result& other) const { return !(*this == other); }

public:  // access
  /// Used to determine if the result is not an error.
  ///
  /// @return true  - object holds a valid data of type T
  ///         false - object does not hold valid data of type T
  [[nodiscard]] bool isOk() const noexcept { return std::holds_alternative<T>(value_); }

  /// Used to determine if the result is an error.
  ///
  /// @return true  - object holds an error of type E
  ///         false - object does not hold an error of type E
  [[nodiscard]] bool isError() const noexcept { return std::holds_alternative<E>(value_); }

  /// Used to determine if the result is not an error.
  ///
  /// @return true  - object holds a valid data of type T
  ///         false - object does not hold valid data of type T
  explicit operator bool() const noexcept { return isOk(); }

  /// Used to determine the return value.
  /// If the result indicates an error this method will return defaultValue.
  ///
  /// @param defaultVal The value to be used in case of holding an error.
  /// @return defaultVal if this object holds an error, the stored success value otherwise.
  template <typename U = T>
  [[nodiscard]] const impl::NonVoidT<U>& getValueOr(const U& defaultVal) const noexcept;

  /// Used to determine the success value, given that there is no error.
  ///
  /// @warning if this object holds an error, calling this function will cause program *to
  /// terminate*.
  ///
  /// @pre isOk() || !isError()
  /// @return the success value stored in this object.
  template <typename U = T>
  [[nodiscard]] const impl::NonVoidT<U>& getValue() const&
  {
    impl::resultExpect(isOk());
    return std::get<T>(value_);
  }

  /// Used to move out the success value, given that there is no error.
  ///
  /// @warning if this object holds an error, calling this function will cause program *to
  /// terminate*.
  ///
  /// @pre isOk() || !isError()
  /// @return the success value stored in this object.
  template <typename U = T>
  [[nodiscard]] impl::NonVoidT<U>&& getValue() &&
  {
    impl::resultExpect(isOk());
    return std::get<T>(std::move(value_));
  }

  /// Used to determine the error value, given that there is an error.
  ///
  /// @warning if this object does not hold an error, calling this function will cause program *to
  /// terminate*.
  ///
  /// @pre isError() || !isOk()
  /// @return the error value stored in this object.
  [[nodiscard]] const E& getError() const
  {
    impl::resultExpect(isError());
    return std::get<E>(value_);
  }

  /// Extracts the value of the correct result, or
  /// terminates the program with a given error message.
  template <typename U = T>
  [[nodiscard]] const impl::NonVoidT<U>& expect(std::string_view errorMsg = {}) const noexcept
  {
    impl::resultExpect(isOk(), errorMsg);
    return std::get<T>(value_);
  }

private:
  std::variant<E, T> value_;
};

/// True or false result
using BoolResult = Result<void, std::monostate>;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

/// Helper (syntactic sugar) to create Results that indicate success.
template <typename T, typename CleanT = std::decay_t<T>>
impl::Ok<CleanT> Ok(T&& val) noexcept;  // NOLINT(readability-identifier-naming)

/// Helper (syntactic sugar) to create Results that indicate error.
template <typename E, typename CleanE = std::decay_t<E>>
impl::Err<CleanE> Err(E&& val) noexcept;  // NOLINT(readability-identifier-naming)

/// If T is void, use the void specialization of Ok.
impl::Ok<void> Ok() noexcept;  // NOLINT(readability-identifier-naming)

/// If E is void, use the void specialization of Err.
impl::Err<void> Err() noexcept;  // NOLINT(readability-identifier-naming)

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T, typename E>
template <typename U>
inline const impl::NonVoidT<U>& Result<T, E>::getValueOr(const U& defaultVal) const noexcept
{
  if (const auto* maybeVal = std::get_if<T>(&value_))
  {
    return *maybeVal;
  }

  return defaultVal;
}

/// Specialization for Results that just hold error data.
template <typename E>
class Result<void, E> final
{
  static_assert(!::std::is_void_v<E>, "void error type is not allowed");

public:
  using ValueType = void;
  using ErrorType = E;

public:
  Result(impl::Ok<void>&& arg) noexcept: ok_(true)  // NOLINT(hicpp-explicit-conversions)
  {
    std::ignore = arg;
  }

  template <typename T>
  Result(Result<T, E>&& other) noexcept  // NOLINT(hicpp-explicit-conversions)
    : ok_(other.isOk())
  {
  }

  Result(impl::Err<E>&& err) noexcept  // NOLINT(hicpp-explicit-conversions)
    : ok_(false), error_(std::move(err.val))
  {
  }

public:  // defaults
  Result(Result&& other) noexcept = default;
  Result(const Result& other) noexcept = default;
  ~Result() noexcept = default;
  Result& operator=(const Result& other) noexcept = default;
  Result& operator=(Result&& other) noexcept = default;

public:  // comparison
  bool operator==(const Result& other) const noexcept { return ok_ == other.ok_ && error_ == other.error_; }

  bool operator!=(const Result& other) const noexcept { return !(*this == other); }

public:  // access
  [[nodiscard]] bool isOk() const noexcept { return ok_; }

  [[nodiscard]] bool isError() const noexcept { return !isOk(); }

  explicit operator bool() const noexcept { return isOk(); }

  [[nodiscard]] const E& getError() const noexcept
  {
    impl::resultExpect(isError());

    return error_;
  }

private:
  bool ok_;
  E error_ = {};
};

/// Specialization for Results that just hold success indication.
template <>
class Result<void, std::monostate> final
{
public:
  using ValueType = void;
  using ErrorType = std::monostate;

public:
  Result(impl::Ok<void>&& arg) noexcept: ok_(true)  // NOLINT(hicpp-explicit-conversions)
  {
    std::ignore = arg;
  }

  Result(impl::Err<void>&& arg) noexcept: ok_(false)  // NOLINT(hicpp-explicit-conversions)
  {
    std::ignore = arg;
  }

public:  // defaults
  Result(Result&& other) noexcept = default;
  Result(const Result& other) noexcept = default;
  ~Result() noexcept = default;
  Result& operator=(const Result& other) noexcept = default;
  Result& operator=(Result&& other) noexcept = default;

public:  // comparison
  bool operator==(const Result& other) const noexcept { return ok_ == other.ok_; }

  bool operator!=(const Result& other) const noexcept { return !(*this == other); }

public:  // access
  [[nodiscard]] bool isOk() const noexcept { return ok_; }

  [[nodiscard]] bool isError() const noexcept { return !isOk(); }

  explicit operator bool() const noexcept { return isOk(); }

private:
  bool ok_;
};

template <typename T, typename CleanT>
inline impl::Ok<CleanT> Ok(T&& val) noexcept  // NOLINT(readability-identifier-naming)
{
  return impl::Ok<CleanT>(std::forward<T>(val));
}

template <typename E, typename CleanE>
inline impl::Err<CleanE> Err(E&& val) noexcept  // NOLINT(readability-identifier-naming)
{
  return impl::Err<CleanE>(std::forward<E>(val));
}

[[nodiscard]] inline impl::Ok<void> Ok() noexcept  // NOLINT(readability-identifier-naming)
{
  return {};
}

[[nodiscard]] inline impl::Err<void> Err() noexcept  // NOLINT(readability-identifier-naming)
{
  return {};
}

}  // namespace sen

#endif  // SEN_CORE_BASE_RESULT_H
