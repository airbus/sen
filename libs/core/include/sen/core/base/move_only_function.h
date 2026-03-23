// === move_only_function.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_MOVE_ONLY_FUNCTION_H
#define SEN_CORE_BASE_MOVE_ONLY_FUNCTION_H

// std
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#if defined(__cpp_lib_move_only_function)
namespace sen::std_util
{
using std::move_only_function;
}  // namespace sen::std_util
#else
namespace sen::std_util::detail
{

// This function implements std::invoke_r as a forward port from C++23 following P2136R3
template <class R, class F, class... Args, std::enable_if_t<std::is_invocable_r_v<R, F, Args...>, bool> = true>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
constexpr R invoke_r(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_r_v<R, F, Args...>)
{
  if constexpr (std::is_void_v<R>)
  {
    std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
  }
  else
  {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
  }
}

template <typename T>
struct IsInPlaceType: std::false_type
{
};
template <typename T>
struct IsInPlaceType<std::in_place_type_t<T>>: std::true_type
{
};

// These classes implement std::move_only_function as a forward port from C++23 following P0288R9
template <typename... Signature>
class MoveOnlyFunctionImpl;

// NOLINTNEXTLINE(hicpp-special-member-functions): not needed as this base class is hidden behind MoveOnlyFunctionImpl
class MoveOnlyFunctionBase
{
protected:
  /// Returns true if the given CallableType can be initialized without throwing given the specified ArgTypes.
  template <typename CallableType, typename... ArgTypes>
  static constexpr bool isNothrowInit() noexcept
  {
    if constexpr (isSmallSizeOptimized<CallableType>)
    {
      return std::is_nothrow_constructible_v<CallableType, ArgTypes...>;
    }
    return false;
  }

  /// Casts the smallsize/allocated callable into the specified type.
  template <typename CallableType, typename QualifiedThisType>
  static CallableType* getCallableAs(QualifiedThisType* thisPtr) noexcept
  {
    if constexpr (isSmallSizeOptimized<std::remove_const_t<CallableType>>)
    {
      return static_cast<CallableType*>(thisPtr->buffer_.rawBufferAddress());
    }
    else
    {
      // * union access valid due to checked alignment requirements
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
      return static_cast<CallableType*>(thisPtr->buffer_.ptrToStoredCallable);
    }
  }

protected:
  MoveOnlyFunctionBase() noexcept: resourceHandler_(emptyResourceHandler) {}
  MoveOnlyFunctionBase(MoveOnlyFunctionBase&& other) noexcept
  {
    resourceHandler_ = std::exchange(other.resourceHandler_, emptyResourceHandler);
    resourceHandler_(buffer_, &other.buffer_);
  }

  template <typename CallableType, typename... ArgTypes>
  void initializeCallable(ArgTypes&&... args) noexcept(isNothrowInit<CallableType, ArgTypes...>())
  {
    if constexpr (isSmallSizeOptimized<CallableType>)
    {
      ::new (buffer_.rawBufferAddress()) CallableType(std::forward<ArgTypes>(args)...);
    }
    else
    {
      // * allocation handled by the resource handler function
      // * union access valid due to checked alignment requirements
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-owning-memory)
      buffer_.ptrToStoredCallable = new CallableType(std::forward<ArgTypes>(args)...);
    }

    resourceHandler_ = &callableResourceHandler<CallableType>;
  }

  MoveOnlyFunctionBase& operator=(MoveOnlyFunctionBase&& other) noexcept
  {
    resourceHandler_(buffer_, nullptr);
    resourceHandler_ = std::exchange(other.resourceHandler_, emptyResourceHandler);
    resourceHandler_(buffer_, &other.buffer_);
    return *this;
  }

  MoveOnlyFunctionBase& operator=(std::nullptr_t) noexcept
  {
    resourceHandler_(buffer_, nullptr);
    resourceHandler_ = emptyResourceHandler;
    return *this;
  }

  ~MoveOnlyFunctionBase() { resourceHandler_(buffer_, nullptr); }

  void swap(MoveOnlyFunctionBase& other) noexcept
  {
    SmallSizeBuffer storage;
    other.resourceHandler_(storage, &other.buffer_);
    resourceHandler_(other.buffer_, &buffer_);
    other.resourceHandler_(buffer_, &storage);
    std::swap(resourceHandler_, other.resourceHandler_);
  }

private:
  struct SmallSizeBuffer
  {  // Allows us to store small callables inside of the function
    static constexpr size_t bufferSize = 3 * sizeof(void*);
    static constexpr size_t bufferAlignment = alignof(void*);

    union  // NOLINT(misc-non-private-member-variables-in-classes)
    {
      void* ptrToStoredCallable;
      alignas(bufferAlignment) std::byte data[bufferSize];
    };

    [[nodiscard]] void* rawBufferAddress()
    {
      return &data[0];  // NOLINT(cppcoreguidelines-pro-type-union-access)
    }
    [[nodiscard]] const void* rawBufferAddress() const
    {
      return &data[0];  // NOLINT(cppcoreguidelines-pro-type-union-access)
    }
  };

  template <typename CallableType>
  static constexpr bool isSmallSizeOptimized =
    alignof(CallableType) <= SmallSizeBuffer::bufferAlignment &&  // Check for correct alignment
    sizeof(CallableType) <= SmallSizeBuffer::bufferSize &&        // Check for correct size
    std::is_nothrow_move_constructible_v<CallableType>;           // Does not throw on construction

  static void emptyResourceHandler(SmallSizeBuffer& target, SmallSizeBuffer* source) noexcept
  {
    std::ignore = target;
    std::ignore = source;
  }

  template <typename CallableType>
  static void callableResourceHandler(SmallSizeBuffer& target, SmallSizeBuffer* source) noexcept
  {
    if constexpr (isSmallSizeOptimized<CallableType>)
    {
      if (source)
      {
        auto* sourceCallable = static_cast<CallableType*>(source->rawBufferAddress());
        ::new (target.rawBufferAddress()) CallableType(std::move(*sourceCallable));
        sourceCallable->~CallableType();
      }
      else
      {
        static_cast<CallableType*>(target.rawBufferAddress())->~CallableType();
      }
    }
    else
    {
      if (source)
      {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
        target.ptrToStoredCallable = source->ptrToStoredCallable;
      }
      else
      {
        // * allocation handled by the resource handler function
        // * union access valid due to checked alignment requirements
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-owning-memory)
        delete static_cast<CallableType*>(target.ptrToStoredCallable);
      }
    }
  }

  SmallSizeBuffer buffer_;
  using ResourceHandlerFunctionType = void (*)(SmallSizeBuffer& target, SmallSizeBuffer* src) noexcept;
  ResourceHandlerFunctionType resourceHandler_;
};

template <typename FunctionType>
inline constexpr bool is_move_only_function_v = false;  // NOLINT(readability-identifier-naming)
template <typename FunctionType>
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr bool is_move_only_function_v<MoveOnlyFunctionImpl<FunctionType>> = true;

}  // namespace sen::std_util::detail

// default
#  define GENERATE_WITH_CV
#  define GENERATE_WITH_REF
#  define GENERATE_WITH_SELF_QUALIFIERS &
#  include "detail/move_only_function_impl.h"

// for const
#  define GENERATE_WITH_CV const
#  define GENERATE_WITH_REF
#  define GENERATE_WITH_SELF_QUALIFIERS GENERATE_WITH_CV&
#  include "detail/move_only_function_impl.h"

// for ref
#  define GENERATE_WITH_CV
#  define GENERATE_WITH_REF             &
#  define GENERATE_WITH_SELF_QUALIFIERS GENERATE_WITH_CV GENERATE_WITH_REF
#  include "detail/move_only_function_impl.h"

// for rvalue ref
#  define GENERATE_WITH_CV
#  define GENERATE_WITH_REF             &&
#  define GENERATE_WITH_SELF_QUALIFIERS GENERATE_WITH_CV GENERATE_WITH_REF
#  include "detail/move_only_function_impl.h"

// for const and ref
#  define GENERATE_WITH_CV              const
#  define GENERATE_WITH_REF             &
#  define GENERATE_WITH_SELF_QUALIFIERS GENERATE_WITH_CV GENERATE_WITH_REF
#  include "detail/move_only_function_impl.h"

// for const and rvalue ref
#  define GENERATE_WITH_CV              const
#  define GENERATE_WITH_REF             &&
#  define GENERATE_WITH_SELF_QUALIFIERS GENERATE_WITH_CV GENERATE_WITH_REF
#  include "detail/move_only_function_impl.h"

namespace sen::std_util
{
/// Move-only callable wrapper equivalent to C++23 `std::move_only_function`.
/// Unlike `std::function`, captured state is never copied â€” only moved.
/// Supports all cv/ref qualifier combinations for the call operator.
/// @tparam FwdArgs Function signature (e.g. `void(int)`, `int() const`).
template <typename... FwdArgs>
using move_only_function = detail::MoveOnlyFunctionImpl<FwdArgs...>;  // NOLINT(readability-identifier-naming)
}  // namespace sen::std_util

#endif

#endif  // SEN_CORE_BASE_MOVE_ONLY_FUNCTION_H
