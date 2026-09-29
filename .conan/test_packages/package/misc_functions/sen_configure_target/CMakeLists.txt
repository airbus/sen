# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Both sources use std::optional. Starting from C++11 proves that the C++17 requirement is applied to the target and
# propagated to its consumer
set(CMAKE_CXX_STANDARD 11)

add_library(cmake_configured_target STATIC src/configured_target.cpp)
sen_configure_target(cmake_configured_target)

foreach(required_property IN ITEMS POSITION_INDEPENDENT_CODE INTERFACE_POSITION_INDEPENDENT_CODE
                                   CXX_STANDARD_REQUIRED
)
  get_target_property(property_value cmake_configured_target ${required_property})
  if(NOT property_value)
    message(FATAL_ERROR "sen_configure_target() did not enable ${required_property}")
  endif()
endforeach()

get_target_property(cxx_extensions cmake_configured_target CXX_EXTENSIONS)
if(NOT
   "${cxx_extensions}"
   STREQUAL
   "OFF"
)
  message(FATAL_ERROR "sen_configure_target() did not disable C++ extensions")
endif()

get_target_property(interface_features cmake_configured_target INTERFACE_COMPILE_FEATURES)
if(NOT
   cxx_std_17
   IN_LIST
   interface_features
)
  message(FATAL_ERROR "sen_configure_target() did not propagate its C++17 requirement")
endif()

# Sen's own build policy must not reach a consumer's target. Every assertion above is a positive one
# -- "did you set what you should" -- and cannot see a flag that should not be there at all.
get_target_property(compile_options cmake_configured_target COMPILE_OPTIONS)
foreach(forbidden IN ITEMS -Wno-array-bounds -Wno-stringop-overread /WX)
  if("${forbidden}" IN_LIST compile_options)
    message(FATAL_ERROR "sen_configure_target() put Sen's own ${forbidden} on a consumer's target")
  endif()
endforeach()

# And the mirror image: a flag that should be here and silently was not, because its gate tested a
# variable only Sen's own build defines.
if(NOT MSVC)
  get_target_property(link_options cmake_configured_target LINK_OPTIONS)
  if(NOT link_options)
    message(FATAL_ERROR "sen_configure_target() set no link options at all")
  endif()
  if(NOT
     "LINKER:--exclude-libs,ALL"
     IN_LIST
     link_options
  )
    message(FATAL_ERROR "sen_configure_target() did not apply --exclude-libs,ALL: ${link_options}")
  endif()
endif()

add_executable(cmake_configured_target_consumer src/main.cpp)
target_link_libraries(cmake_configured_target_consumer PRIVATE cmake_configured_target)
add_test(NAME cmake.sen_configure_target.runtime COMMAND cmake_configured_target_consumer)
