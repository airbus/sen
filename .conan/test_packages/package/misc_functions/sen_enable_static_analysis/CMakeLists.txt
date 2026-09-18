# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_library(cmake_static_analysis_primary OBJECT EXCLUDE_FROM_ALL static_analysis_target.cpp)
add_library(cmake_static_analysis_internal OBJECT EXCLUDE_FROM_ALL static_analysis_target.cpp)
set_property(TARGET cmake_static_analysis_primary PROPERTY OBJECT_LIBRARY cmake_static_analysis_internal)

set(SEN_DISABLE_CLANG_TIDY OFF)
if(MSVC)
  set(analysis_property VS_GLOBAL_EnableClangTidyCodeAnalysis)
  set(expected_analysis_value true)
else()
  # The targets are not built, so this stable sentinel is stored but never executed as clang-tidy.
  set(clang_tidy_path "${CMAKE_COMMAND}")
  set(analysis_property CXX_CLANG_TIDY)
  set(expected_analysis_value "${CMAKE_COMMAND};-extra-arg=-std=c++17")
endif()

sen_enable_static_analysis(cmake_static_analysis_primary)
foreach(analyzed_target IN ITEMS cmake_static_analysis_primary cmake_static_analysis_internal)
  get_target_property(actual_value ${analyzed_target} ${analysis_property})
  if(NOT
     "${actual_value}"
     STREQUAL
     "${expected_analysis_value}"
  )
    message(FATAL_ERROR "sen_enable_static_analysis() did not configure ${analyzed_target}")
  endif()
endforeach()

add_library(cmake_static_analysis_disabled OBJECT EXCLUDE_FROM_ALL static_analysis_target.cpp)
set(SEN_DISABLE_CLANG_TIDY ON)
sen_enable_static_analysis(cmake_static_analysis_disabled)
get_target_property(disabled_value cmake_static_analysis_disabled ${analysis_property})
if(NOT
   "${disabled_value}"
   MATCHES
   "-NOTFOUND$"
)
  message(FATAL_ERROR "sen_enable_static_analysis() ignored SEN_DISABLE_CLANG_TIDY")
endif()
