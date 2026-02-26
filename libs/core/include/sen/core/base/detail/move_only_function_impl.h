// === move_only_function_impl.h =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// NOLINTBEGIN(misc-include-cleaner)
// Due to the include stamping trick, the checker gets confused here

// sen
#include "sen/core/base/move_only_function.h"

// std
#include <cassert>
#include <initializer_list>
#include <type_traits>
// NOLINTEND(misc-include-cleaner)

namespace sen::std_util::detail
{

#if defined(_MSC_VER)
#  define NOEXCEPT_SPECIFIER
#  define NOEXCEPT_SPECIALIZATION
#else
#  define NOEXCEPT_SPECIFIER      noexcept(IsNoExcept)
#  define NOEXCEPT_SPECIALIZATION , bool IsNoExcept
#endif

template <typename ResultType, typename... ArgTypes NOEXCEPT_SPECIALIZATION>
// NOLINTNEXTLINE: prevent confusing between ArgType matching and a c-style cast
class MoveOnlyFunctionImpl<ResultType(ArgTypes...) GENERATE_WITH_CV GENERATE_WITH_REF NOEXCEPT_SPECIFIER>
  : private MoveOnlyFunctionBase
{
#if defined(_MSC_VER)
  // TODO (SEN-1001): implement noexcept detection/specialization support for windows
  static constexpr bool isNoExcept = false;
#else
  static constexpr bool isNoExcept = IsNoExcept;
#endif

  template <typename CallableType>
  // NOLINTNEXTLINE(readability-identifier-naming): follows standard concept naming
  using is_callable = std::conditional_t<isNoExcept,
                                         std::is_nothrow_invocable_r<ResultType, CallableType, ArgTypes...>,
                                         std::is_invocable_r<ResultType, CallableType, ArgTypes...>>;

  template <typename UnqualifiedCallableType>
  // NOLINTNEXTLINE(readability-identifier-naming): extends standard concept naming
  using is_qualified_callable =
    std::conjunction<is_callable<UnqualifiedCallableType GENERATE_WITH_CV GENERATE_WITH_REF>,
                     is_callable<UnqualifiedCallableType GENERATE_WITH_SELF_QUALIFIERS>>;

public:
  using result_type = ResultType;  // NOLINT(readability-identifier-naming): requested by the standard

  MoveOnlyFunctionImpl() noexcept = default;

  // NOLINTNEXTLINE(hicpp-explicit-conversions): needed for automatic nullptr comparisions
  MoveOnlyFunctionImpl(std::nullptr_t) noexcept {}

  MoveOnlyFunctionImpl(MoveOnlyFunctionImpl&& other) noexcept
    : MoveOnlyFunctionBase(static_cast<MoveOnlyFunctionBase&&>(other))
    , activeDispatcherFunction_(std::exchange(other.activeDispatcherFunction_, nullptr))
  {
  }

  template <typename CallableType,
            typename DecayedCallableType = std::decay_t<CallableType>,
            std::enable_if_t<std::conjunction_v<std::negation<std::is_same<DecayedCallableType, MoveOnlyFunctionImpl>>,
                                                std::negation<IsInPlaceType<DecayedCallableType>>,
                                                is_qualified_callable<DecayedCallableType>>,
                             bool> = true>
  // NOLINTNEXTLINE(hicpp-explicit-conversions): required by the standard
  MoveOnlyFunctionImpl(CallableType&& callable) noexcept(isNothrowInit<DecayedCallableType, CallableType>())
  {
    if constexpr (std::is_function_v<std::remove_pointer_t<DecayedCallableType>> ||
                  std::is_member_pointer_v<DecayedCallableType> || is_move_only_function_v<DecayedCallableType>)
    {

#if defined(_MSC_VER) && _MSC_VER < 1932
      // prevent older MSVC versions from trying to compare a reference to a function to a pointer
      if constexpr (!std::is_reference_v<CallableType>)
      {
#endif
        if (callable == nullptr)
        {
          return;
        }
#if defined(_MSC_VER) && _MSC_VER < 1932
      }
#endif
    }
    initializeCallable<DecayedCallableType>(std::forward<CallableType>(callable));
    activeDispatcherFunction_ = &dispatcherFunction<DecayedCallableType>;
  }

  template <typename CallableType,
            typename... FunctionArgTypes,
            std::enable_if_t<std::conjunction_v<std::is_constructible<CallableType, FunctionArgTypes...>,
                                                is_qualified_callable<CallableType>>,
                             bool> = true>
  explicit MoveOnlyFunctionImpl(std::in_place_type_t<CallableType> /*unused*/,
                                FunctionArgTypes&&... args) noexcept(isNothrowInit<CallableType, FunctionArgTypes...>())
    : activeDispatcherFunction_(&dispatcherFunction<CallableType>)
  {
    static_assert(std::is_same_v<std::decay_t<CallableType>, CallableType>);
    initializeCallable<CallableType>(std::forward<FunctionArgTypes>(args)...);
  }

  template <
    typename CallableType,
    typename ILArgTypes,
    typename... FunctionArgTypes,
    std::enable_if_t<
      std::conjunction_v<std::is_constructible<CallableType, std::initializer_list<ILArgTypes>&, FunctionArgTypes...>,
                         is_qualified_callable<CallableType>>,
      bool> = true>
  explicit MoveOnlyFunctionImpl(std::in_place_type_t<CallableType> /*unused*/,
                                std::initializer_list<ILArgTypes> initializeList,
                                FunctionArgTypes&&... args) noexcept(isNothrowInit<CallableType,
                                                                                   std::initializer_list<ILArgTypes>&,
                                                                                   FunctionArgTypes...>())
    : activeDispatcherFunction_(&dispatcherFunction<CallableType>)
  {
    static_assert(std::is_same_v<std::decay_t<CallableType>, CallableType>);
    initializeCallable<CallableType>(initializeList, std::forward<FunctionArgTypes>(args)...);
  }

  MoveOnlyFunctionImpl& operator=(MoveOnlyFunctionImpl&& other) noexcept
  {
    MoveOnlyFunctionBase::operator=(static_cast<MoveOnlyFunctionBase&&>(other));
    activeDispatcherFunction_ = std::exchange(other.activeDispatcherFunction_, nullptr);
    return *this;
  }

  /// Replaces or destroys the target
  MoveOnlyFunctionImpl& operator=(std::nullptr_t) noexcept
  {
    MoveOnlyFunctionBase::operator=(nullptr);
    activeDispatcherFunction_ = nullptr;
    return *this;
  }

  ~MoveOnlyFunctionImpl() = default;

  /// Invokes the target
  ResultType operator()(ArgTypes... args) GENERATE_WITH_CV GENERATE_WITH_REF noexcept(isNoExcept)
  {
    assert(*this != nullptr && "Trying to execute empty callable object.");
    return activeDispatcherFunction_(this, std::forward<ArgTypes>(args)...);
  }

  /// Checks if the std::move_only_function has a target
  explicit operator bool() const noexcept { return activeDispatcherFunction_ != nullptr; }

  /// Compares a std::move_only_function with nullptr
  friend bool operator==(const MoveOnlyFunctionImpl& lhs, std::nullptr_t) noexcept
  {
    return lhs.activeDispatcherFunction_ == nullptr;
  }

  /// Compares a std::move_only_function with nullptr
  friend bool operator!=(const MoveOnlyFunctionImpl& lhs, std::nullptr_t) noexcept
  {
    return lhs.activeDispatcherFunction_ != nullptr;
  }

  /// Specializes the std::swap algorithm
  friend void swap(MoveOnlyFunctionImpl& lhs, MoveOnlyFunctionImpl& rhs) noexcept { lhs.swap(rhs); }

  /// Swaps the target of the std::move_only_function objects
  void swap(MoveOnlyFunctionImpl& other) noexcept
  {
    MoveOnlyFunctionBase::swap(other);
    std::swap(activeDispatcherFunction_, other.activeDispatcherFunction_);
  }

private:
  template <typename ParameterType>
  using SanitizedParameterType = std::conditional_t<std::is_scalar_v<ParameterType>, ParameterType, ParameterType&&>;

  template <typename Tp>
  static ResultType dispatcherFunction(MoveOnlyFunctionBase GENERATE_WITH_CV* self,
                                       SanitizedParameterType<ArgTypes>... args) noexcept(isNoExcept)
  {
    return invoke_r<ResultType>(
      std::forward<Tp GENERATE_WITH_SELF_QUALIFIERS>(*getCallableAs<Tp GENERATE_WITH_CV>(self)),
      std::forward<SanitizedParameterType<ArgTypes>>(args)...);
  }

  using DispatcherFunctionType = ResultType (*)(MoveOnlyFunctionBase GENERATE_WITH_CV*,
                                                SanitizedParameterType<ArgTypes>...) noexcept(isNoExcept);
  DispatcherFunctionType activeDispatcherFunction_ = nullptr;
};

}  // namespace sen::std_util::detail

#undef GENERATE_WITH_CV
#undef GENERATE_WITH_REF
#undef GENERATE_WITH_SELF_QUALIFIERS
#undef NOEXCEPT_SPECIFIER
#undef NOEXCEPT_SPECIALIZATION
