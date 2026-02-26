// === compiler_macros.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_COMPILER_MACROS_H
#define SEN_CORE_BASE_COMPILER_MACROS_H

/// \addtogroup macros
/// @{

//--------------------------------------------------------------------------------------------------------------
// SEN_STRINGIFY
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_STRINGIFY_EX)
#  undef SEN_STRINGIFY_EX
#endif
#define SEN_STRINGIFY_EX(x) #x  // NOLINT

#if defined(SEN_STRINGIFY)
#  undef SEN_STRINGIFY
#endif
#define SEN_STRINGIFY(x) SEN_STRINGIFY_EX(x)  // NOLINT

//--------------------------------------------------------------------------------------------------------------
// SEN_CONCAT
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_CONCAT_EX)
#  undef SEN_CONCAT_EX
#endif
#define SEN_CONCAT_EX(a, b) a##b  // NOLINT

#if defined(SEN_CONCAT)
#  undef SEN_CONCAT
#endif
#define SEN_CONCAT(a, b) SEN_CONCAT_EX(a, b)  // NOLINT

//--------------------------------------------------------------------------------------------------------------
// SEN_VERSION_ENCODE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_VERSION_ENCODE)
#  undef SEN_VERSION_ENCODE
#endif
#define SEN_VERSION_ENCODE(major, minor, revision) (((major) * 1000000) + ((minor) * 1000) + (revision))  // NOLINT

//--------------------------------------------------------------------------------------------------------------
// SEN_VERSION_DECODE_MAJOR
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_VERSION_DECODE_MAJOR)
#  undef SEN_VERSION_DECODE_MAJOR
#endif
#define SEN_VERSION_DECODE_MAJOR(version) ((version) / 1000000)  // NOLINT

//--------------------------------------------------------------------------------------------------------------
// SEN_VERSION_DECODE_MINOR
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_VERSION_DECODE_MINOR)
#  undef SEN_VERSION_DECODE_MINOR
#endif
#define SEN_VERSION_DECODE_MINOR(version) (((version) % 1000000) / 1000)  // NOLINT

//--------------------------------------------------------------------------------------------------------------
// SEN_VERSION_DECODE_REVISION
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_VERSION_DECODE_REVISION)
#  undef SEN_VERSION_DECODE_REVISION
#endif
#define SEN_VERSION_DECODE_REVISION(version) ((version) % 1000)  // NOLINT

//--------------------------------------------------------------------------------------------------------------
// SEN_MSVC_VERSION, SEN_MSVC_VERSION_CHECK
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_MSVC_VERSION)
#  undef SEN_MSVC_VERSION
#endif
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000) && !defined(__ICL)
// NOLINTNEXTLINE
#  define SEN_MSVC_VERSION                                                                                             \
    SEN_VERSION_ENCODE(_MSC_FULL_VER / 10000000, (_MSC_FULL_VER % 10000000) / 100000, (_MSC_FULL_VER % 100000) / 100)
#elif defined(_MSC_FULL_VER) && !defined(__ICL)
// NOLINTNEXTLINE
#  define SEN_MSVC_VERSION                                                                                             \
    SEN_VERSION_ENCODE(_MSC_FULL_VER / 1000000, (_MSC_FULL_VER % 1000000) / 10000, (_MSC_FULL_VER % 10000) / 10)
#elif defined(_MSC_VER) && !defined(__ICL)
#  define SEN_MSVC_VERSION SEN_VERSION_ENCODE(_MSC_VER / 100, _MSC_VER % 100, 0)  // NOLINT
#endif

#if defined(SEN_MSVC_VERSION_CHECK)
#  undef SEN_MSVC_VERSION_CHECK
#endif
#if !defined(SEN_MSVC_VERSION)
#  define SEN_MSVC_VERSION_CHECK(major, minor, patch) (0)  // NOLINT
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
// NOLINTNEXTLINE
#  define SEN_MSVC_VERSION_CHECK(major, minor, patch)                                                                  \
    (_MSC_FULL_VER >= ((major * 10000000) + (minor * 100000) + (patch)))
#elif defined(_MSC_VER) && (_MSC_VER >= 1200)
// NOLINTNEXTLINE
#  define SEN_MSVC_VERSION_CHECK(major, minor, patch) (_MSC_FULL_VER >= ((major * 1000000) + (minor * 10000) + (patch)))
#else
#  define SEN_MSVC_VERSION_CHECK(major, minor, patch) (_MSC_VER >= ((major * 100) + (minor)))  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_GCC_VERSION, SEN_GCC_VERSION_CHECK
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_GNUC_VERSION)
#  undef SEN_GNUC_VERSION
#endif
#if defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)
#  define SEN_GNUC_VERSION SEN_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)  // NOLINT
#elif defined(__GNUC__)
#  define SEN_GNUC_VERSION SEN_VERSION_ENCODE(__GNUC__, __GNUC_MINOR__, 0)  // NOLINT
#endif

#if defined(SEN_GCC_VERSION)
#  undef SEN_GCC_VERSION
#endif
#if defined(SEN_GNUC_VERSION) && !defined(__clang__)
#  define SEN_GCC_VERSION SEN_GNUC_VERSION  // NOLINT
#endif

#if defined(SEN_GCC_VERSION_CHECK)
#  undef SEN_GCC_VERSION_CHECK
#endif
#if defined(SEN_GCC_VERSION)
// NOLINTNEXTLINE
#  define SEN_GCC_VERSION_CHECK(major, minor, patch) (SEN_GCC_VERSION >= SEN_VERSION_ENCODE(major, minor, patch))
// NOLINTNEXTLINE
#  define SEN_GCC_VERSION_CHECK_SMALLER(major, minor, patch) (SEN_GCC_VERSION < SEN_VERSION_ENCODE(major, minor, patch))
#else
// NOLINTNEXTLINE
#  define SEN_GCC_VERSION_CHECK(major, minor, patch)         (0)
// NOLINTNEXTLINE
#  define SEN_GCC_VERSION_CHECK_SMALLER(major, minor, patch) (0)
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_HAS_ATTRIBUTE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_HAS_ATTRIBUTE)
#  undef SEN_HAS_ATTRIBUTE
#endif
#if defined(__has_attribute)
#  define SEN_HAS_ATTRIBUTE(attribute) __has_attribute(attribute)  // NOLINT
#else
#  define SEN_HAS_ATTRIBUTE(attribute) (0)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_HAS_CPP_ATTRIBUTE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_HAS_CPP_ATTRIBUTE)
#  undef SEN_HAS_CPP_ATTRIBUTE
#endif
#if defined(__has_cpp_attribute) && defined(__cplusplus)
#  define SEN_HAS_CPP_ATTRIBUTE(attribute) __has_cpp_attribute(attribute)  // NOLINT
#else
#  define SEN_HAS_CPP_ATTRIBUTE(attribute) (0)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_HAS_BUILTIN
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_HAS_BUILTIN)
#  undef SEN_HAS_BUILTIN
#endif
#if defined(__has_builtin)
#  define SEN_HAS_BUILTIN(builtin) __has_builtin(builtin)  // NOLINT
#else
#  define SEN_HAS_BUILTIN(builtin) (0)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_HAS_EXTENSION
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_HAS_EXTENSION)
#  undef SEN_HAS_EXTENSION
#endif
#if defined(__has_extension)
#  define SEN_HAS_EXTENSION(extension) __has_extension(extension)  // NOLINT
#else
#  define SEN_HAS_EXTENSION(extension) (0)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_DEPRECATED, SEN_DEPRECATED_FOR
//--------------------------------------------------------------------------------------------------------------

// clang-format off

#if defined(SEN_DEPRECATED)
#  undef SEN_DEPRECATED
#endif
#if defined(SEN_DEPRECATED_FOR)
#  undef SEN_DEPRECATED_FOR
#endif
#if SEN_MSVC_VERSION_CHECK(14, 0, 0)
#  define SEN_DEPRECATED(since) __declspec(deprecated("Since " #  since))  // NOLINT
// NOLINTNEXTLINE
#  define SEN_DEPRECATED_FOR(since, replacement) __declspec(deprecated("Since " #  since "; use " #  replacement))
#elif SEN_HAS_EXTENSION(attribute_deprecated_with_message) || SEN_GCC_VERSION_CHECK(4, 5, 0)
#  define SEN_DEPRECATED(since) __attribute__((__deprecated__("Since " #  since)))  // NOLINT
// NOLINTNEXTLINE
#  define SEN_DEPRECATED_FOR(since, replacement)                                                                    \
    __attribute__((__deprecated__("Since " #since "; use " #replacement)))  // NOLINT
#elif defined(__cplusplus) && (__cplusplus >= 201402L)
#  define SEN_DEPRECATED(since)                  [[deprecated("Since " #  since)]]                          // NOLINT
#  define SEN_DEPRECATED_FOR(since, replacement) [[deprecated("Since " #  since "; use " #  replacement)]]  // NOLINT
#elif SEN_HAS_ATTRIBUTE(deprecated) || SEN_GCC_VERSION_CHECK(3, 1, 0)
#  define SEN_DEPRECATED(since)                  __attribute__((__deprecated__))  // NOLINT
#  define SEN_DEPRECATED_FOR(since, replacement) __attribute__((__deprecated__))  // NOLINT
#elif SEN_MSVC_VERSION_CHECK(13, 10, 0)
#  define SEN_DEPRECATED(since)                  __declspec(deprecated)  // NOLINT
#  define SEN_DEPRECATED_FOR(since, replacement) __declspec(deprecated)  // NOLINT
#else
#  define SEN_DEPRECATED(since)                   // NOLINT
#  define SEN_DEPRECATED_FOR(since, replacement)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_UNAVAILABLE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_UNAVAILABLE)
#  undef SEN_UNAVAILABLE
#endif
#if SEN_HAS_ATTRIBUTE(warning) || SEN_GCC_VERSION_CHECK(4, 3, 0)
// NOLINTNEXTLINE
#  define SEN_UNAVAILABLE(available_since) __attribute__((__warning__("Not available until " #  available_since)))
#else
#  define SEN_UNAVAILABLE(available_since)  // NOLINT
#endif

// clang-format on

//--------------------------------------------------------------------------------------------------------------
// SEN_PRINTF_FORMAT
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_PRINTF_FORMAT)
#  undef SEN_PRINTF_FORMAT
#endif
#if SEN_HAS_ATTRIBUTE(format) || SEN_GCC_VERSION_CHECK(3, 1, 0)
// NOLINTNEXTLINE
#  define SEN_PRINTF_FORMAT(string_idx, first_to_check)                                                                \
    __attribute__((__format__(__printf__, string_idx, first_to_check)))
#else
#  define SEN_PRINTF_FORMAT(string_idx, first_to_check)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_ALWAYS_INLINE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_ALWAYS_INLINE)
#  undef SEN_ALWAYS_INLINE
#endif
#if SEN_HAS_ATTRIBUTE(always_inline) || SEN_GCC_VERSION_CHECK(4, 0, 0)
#  define SEN_ALWAYS_INLINE __attribute__((__always_inline__)) inline  // NOLINT
#elif SEN_MSVC_VERSION_CHECK(12, 0, 0)
#  define SEN_ALWAYS_INLINE __forceinline  // NOLINT
#else
#  define SEN_ALWAYS_INLINE inline  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_PREDICT, SEN_LIKELY, SEN_UNLIKELY, SEN_UNPREDICTABLE, SEN_PREDICT_TRUE, SEN_PREDICT_FALSE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_PREDICT)
#  undef SEN_PREDICT
#endif
#if defined(SEN_LIKELY)
#  undef SEN_LIKELY
#endif
#if defined(SEN_UNLIKELY)
#  undef SEN_UNLIKELY
#endif
#if defined(SEN_UNPREDICTABLE)
#  undef SEN_UNPREDICTABLE
#endif
#if SEN_HAS_BUILTIN(__builtin_unpredictable)
#  define SEN_UNPREDICTABLE(expr) __builtin_unpredictable((expr))  // NOLINT
#endif
#if SEN_HAS_BUILTIN(__builtin_expect_with_probability) || SEN_GCC_VERSION_CHECK(9, 0, 0)

// NOLINTNEXTLINE
#  define SEN_PREDICT(expr, value, probability) __builtin_expect_with_probability((expr), (value), (probability))
// NOLINTNEXTLINE
#  define SEN_PREDICT_TRUE(expr, probability) __builtin_expect_with_probability(!!(expr), 1, (probability))
// NOLINTNEXTLINE
#  define SEN_PREDICT_FALSE(expr, probability) __builtin_expect_with_probability(!!(expr), 0, (probability))

#  define SEN_LIKELY(expr)   __builtin_expect(!!(expr), 1)  // NOLINT
#  define SEN_UNLIKELY(expr) __builtin_expect(!!(expr), 0)  // NOLINT
#elif SEN_HAS_BUILTIN(__builtin_expect) || SEN_GCC_VERSION_CHECK(3, 0, 0)
// NOLINTNEXTLINE
#  define SEN_PREDICT(expr, expected, probability)                                                                     \
    (((probability) >= 0.9) ? __builtin_expect((expr), (expected)) : (static_cast<void>(expected), (expr)))
// NOLINTNEXTLINE
#  define SEN_PREDICT_TRUE(expr, probability)                                                                          \
    (__extension__({                                                                                                   \
      double senProbability = (probability);                                                                           \
      ((senProbability >= 0.9) ? __builtin_expect(!!(expr), 1)                                                         \
                               : ((senProbability <= 0.1) ? __builtin_expect(!!(expr), 0) : !!(expr)));                \
    }))
// NOLINTNEXTLINE
#  define SEN_PREDICT_FALSE(expr, probability)                                                                         \
    (__extension__({                                                                                                   \
      double senProbability = (probability);                                                                           \
      ((senProbability >= 0.9) ? __builtin_expect(!!(expr), 0)                                                         \
                               : ((senProbability <= 0.1) ? __builtin_expect(!!(expr), 1) : !!(expr)));                \
    }))

#  define SEN_LIKELY(expr)   __builtin_expect(!!(expr), 1)  // NOLINT
#  define SEN_UNLIKELY(expr) __builtin_expect(!!(expr), 0)  // NOLINT
#else
#  define SEN_PREDICT(expr, expected, probability) (static_cast<void>(expected), (expr))  // NOLINT
#  define SEN_PREDICT_TRUE(expr, probability)      (!!(expr))                             // NOLINT
#  define SEN_PREDICT_FALSE(expr, probability)     (!!(expr))                             // NOLINT
#  define SEN_LIKELY(expr)                         (!!(expr))                             // NOLINT
#  define SEN_UNLIKELY(expr)                       (!!(expr))                             // NOLINT
#endif
#if !defined(SEN_UNPREDICTABLE)
#  define SEN_UNPREDICTABLE(expr) SEN_PREDICT(expr, 1, 0.5)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_EXPORT, SEN_IMPORT, SEN_PRIVATE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_EXPORT)
#  undef SEN_EXPORT
#endif
#if defined(SEN_IMPORT)
#  undef SEN_IMPORT
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#  define SEN_PRIVATE                                   // NOLINT
#  define SEN_EXPORT             __declspec(dllexport)  // NOLINT
#  define SEN_MAYBE_EXPORT(flag) SEN_MAYBE_EXPORT_##flag
#  define SEN_MAYBE_EXPORT_true  SEN_EXPORT  // NOLINT
#  define SEN_MAYBE_EXPORT_false             // NOLINT
#  define SEN_IMPORT __declspec(dllimport)   // NOLINT
#else
#  if SEN_HAS_ATTRIBUTE(visibility) || SEN_GCC_VERSION_CHECK(3, 3, 0)
#    define SEN_PRIVATE            __attribute__((__visibility__("hidden")))   // NOLINT
#    define SEN_EXPORT             __attribute__((__visibility__("default")))  // NOLINT
#    define SEN_MAYBE_EXPORT(flag) SEN_MAYBE_EXPORT_##flag
#    define SEN_MAYBE_EXPORT_true  SEN_EXPORT  // NOLINT
#    define SEN_MAYBE_EXPORT_false             // NOLINT
#  else
#    define SEN_PRIVATE  // NOLINT
#    define SEN_EXPORT   // NOLINT
#  endif
#  define SEN_IMPORT  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_FALL_THROUGH
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_FALL_THROUGH)
#  undef SEN_FALL_THROUGH
#endif
#if SEN_HAS_ATTRIBUTE(fallthrough) || SEN_GCC_VERSION_CHECK(7, 0, 0)
#  define SEN_FALL_THROUGH __attribute__((__fallthrough__))  // NOLINT
#elif SEN_HAS_CPP_ATTRIBUTE(fallthrough)
#  define SEN_FALL_THROUGH [[fallthrough]]  // NOLINT
#elif defined(__fallthrough)                /* SAL */
#  define SEN_FALL_THROUGH __fallthrough    // NOLINT
#else
#  define SEN_FALL_THROUGH  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_UNREACHABLE
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_UNREACHABLE)
#  undef SEN_UNREACHABLE
#endif
#if defined(SEN_ASSUME)
#  undef SEN_ASSUME
#endif
#if SEN_MSVC_VERSION_CHECK(13, 10, 0)
#  define SEN_ASSUME(expr) __assume(expr)  // NOLINT
#elif SEN_HAS_BUILTIN(__builtin_assume)
#  define SEN_ASSUME(expr) __builtin_assume(expr)  // NOLINT
#endif
#if SEN_HAS_BUILTIN(__builtin_unreachable) || SEN_GCC_VERSION_CHECK(4, 5, 0)
#  define SEN_UNREACHABLE() __builtin_unreachable()  // NOLINT
#elif defined(SEN_ASSUME)
#  define SEN_UNREACHABLE() SEN_ASSUME(0)  // NOLINT
#endif
#if !defined(SEN_ASSUME)
#  if defined(SEN_UNREACHABLE)
#    define SEN_ASSUME(expr) static_cast<void>((expr) ? 1 : (SEN_UNREACHABLE(), 1))  // NOLINT
#  else
#    define SEN_ASSUME(expr) static_cast<void>(expr)  // NOLINT
#  endif
#endif

#if !defined(SEN_UNREACHABLE)
#  define SEN_UNREACHABLE() SEN_ASSUME(0)  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_RETURNS_NON_NULL
//--------------------------------------------------------------------------------------------------------------

#if defined(SEN_RETURNS_NON_NULL)
#  undef SEN_RETURNS_NON_NULL
#endif
#if SEN_HAS_ATTRIBUTE(returns_nonnull) || SEN_GCC_VERSION_CHECK(4, 9, 0)
#  define SEN_RETURNS_NON_NULL __attribute__((__returns_nonnull__))  // NOLINT
#elif defined(_Ret_notnull_)
#  define SEN_RETURNS_NON_NULL _Ret_notnull_  // NOLINT
#else
#  define SEN_RETURNS_NON_NULL  // NOLINT
#endif

//--------------------------------------------------------------------------------------------------------------
// SEN_COPY_CONSTRUCT, SEN_COPY_ASSIGN, SEN_MOVE_CONSTRUCT, SEN_MOVE_ASSIGN
//--------------------------------------------------------------------------------------------------------------

#define SEN_COPY_CONSTRUCT(classname) classname(const classname& other)                 // NOLINT
#define SEN_COPY_ASSIGN(classname)    classname& operator=(const classname& other)      // NOLINT
#define SEN_MOVE_CONSTRUCT(classname) classname(classname&& other) noexcept             // NOLINT
#define SEN_MOVE_ASSIGN(classname)    classname& operator=(classname&& other) noexcept  // NOLINT

//--------------------------------------------------------------------------------------------------------------
// SEN_NOCOPY_NOMOVE
//--------------------------------------------------------------------------------------------------------------

// NOLINTNEXTLINE
#define SEN_NOCOPY_NOMOVE(classname)                                                                                   \
  SEN_COPY_CONSTRUCT(classname) = delete;                                                                              \
  SEN_COPY_ASSIGN(classname) = delete;                                                                                 \
  SEN_MOVE_CONSTRUCT(classname) = delete;                                                                              \
  SEN_MOVE_ASSIGN(classname) = delete;

//--------------------------------------------------------------------------------------------------------------
// SEN_MOVE_ONLY
//--------------------------------------------------------------------------------------------------------------

// NOLINTNEXTLINE
#define SEN_MOVE_ONLY(classname)                                                                                       \
public:                                                                                                                \
  SEN_COPY_CONSTRUCT(classname) = delete;                                                                              \
  SEN_COPY_ASSIGN(classname) = delete;                                                                                 \
  SEN_MOVE_CONSTRUCT(classname) = default;                                                                             \
  SEN_MOVE_ASSIGN(classname) = default;                                                                                \
                                                                                                                       \
private:

//--------------------------------------------------------------------------------------------------------------
// SEN_COPY_MOVE
//--------------------------------------------------------------------------------------------------------------

// NOLINTNEXTLINE
#define SEN_COPY_MOVE(classname)                                                                                       \
public:                                                                                                                \
  SEN_COPY_CONSTRUCT(classname) = default;                                                                             \
  SEN_COPY_ASSIGN(classname) = default;                                                                                \
  SEN_MOVE_CONSTRUCT(classname) = default;                                                                             \
  SEN_MOVE_ASSIGN(classname) = default;                                                                                \
                                                                                                                       \
private:

//--------------------------------------------------------------------------------------------------------------
// SEN_PRIVATE_TAG
//--------------------------------------------------------------------------------------------------------------

#define SEN_PRIVATE_TAG                                                                                                \
private:                                                                                                               \
  struct Private                                                                                                       \
  {                                                                                                                    \
    explicit Private() = default;                                                                                      \
  };

/// @}

#endif  // SEN_CORE_BASE_COMPILER_MACROS_H
