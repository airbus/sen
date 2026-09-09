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

add_executable(cmake_configured_target_consumer src/main.cpp)
target_link_libraries(cmake_configured_target_consumer PRIVATE cmake_configured_target)
add_test(NAME cmake.sen_configure_target.runtime COMMAND cmake_configured_target_consumer)
