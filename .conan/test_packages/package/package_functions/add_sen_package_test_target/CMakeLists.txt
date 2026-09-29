# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# TEST_TARGET splits the sources into an object library, and everything else links that library
# PRIVATE. Anything the object library declares PUBLIC stops there, so both final targets have to
# name sen::kernel themselves.
add_sen_package(
  TARGET probe_package
  TEST_TARGET probe_package_test
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Checks what the object-library split leaves in the link interface"
  SOURCES src/probe.h src/probe.cpp
  NO_SCHEMA
)

# The split itself, first. Without it there is no PRIVATE hop to lose the kernel through, and the
# assertions below would hold for a reason that has nothing to do with them.
get_target_property(probe_obj_type probe_package_obj TYPE)
if(NOT
   "${probe_obj_type}"
   STREQUAL
   "OBJECT_LIBRARY"
)
  message(FATAL_ERROR "add_sen_package(TEST_TARGET) built ${probe_obj_type}; expected OBJECT_LIBRARY")
endif()

foreach(probe_target IN ITEMS probe_package probe_package_test)
  get_target_property(probe_interface_links ${probe_target} INTERFACE_LINK_LIBRARIES)
  if(NOT
     sen::kernel
     IN_LIST
     probe_interface_links
  )
    message(
      FATAL_ERROR "add_sen_package(TEST_TARGET) left sen::kernel out of ${probe_target}'s link interface "
                  "(${probe_interface_links}), so a consumer resolves the kernel symbols itself"
    )
  endif()
endforeach()

get_target_property(probe_test_type probe_package_test TYPE)
if(NOT
   "${probe_test_type}"
   STREQUAL
   "STATIC_LIBRARY"
)
  message(FATAL_ERROR "add_sen_package(TEST_TARGET) built ${probe_test_type}; expected STATIC_LIBRARY")
endif()
