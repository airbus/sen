// === sanitizer_default_options.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// The sanitizer runtimes call these at startup and take what they return as their defaults.
// They exist because CMake's GoogleTestAddTests.cmake expands test properties unquoted, which
// flattens a list-valued ENVIRONMENT and drops everything after its first entry. Compiled into
// each binary, never offered from a library: the linker drops an unreferenced one silently.

#if defined(SEN_ASAN_DEFAULT_OPTIONS)
extern "C" const char* __asan_default_options() { return SEN_ASAN_DEFAULT_OPTIONS; }
#endif

#if defined(SEN_LSAN_DEFAULT_OPTIONS)
extern "C" const char* __lsan_default_options() { return SEN_LSAN_DEFAULT_OPTIONS; }
#endif

#if defined(SEN_UBSAN_DEFAULT_OPTIONS)
extern "C" const char* __ubsan_default_options() { return SEN_UBSAN_DEFAULT_OPTIONS; }
#endif

#if defined(SEN_TSAN_DEFAULT_OPTIONS)
extern "C" const char* __tsan_default_options() { return SEN_TSAN_DEFAULT_OPTIONS; }
#endif
