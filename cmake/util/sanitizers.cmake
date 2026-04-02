# === sanitizers.cmake =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fasynchronous-unwind-tables")

  if(SEN_USE_SANITIZER STREQUAL ASanUBSan)
    message(STATUS "Enabling sanitizers: ASan, UBSan")

    get_filename_component(LSAN_SUPPRESSION_FILE cmake/util/lsan_ignorelist.txt ABSOLUTE)
    get_filename_component(ASAN_SUPPRESSION_FILE cmake/util/asan_ignorelist.txt ABSOLUTE)

    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
  elseif(SEN_USE_SANITIZER STREQUAL Thread)
    message(STATUS "Enabling sanitizers: Thread")

    get_filename_component(TSAN_SUPPRESSION_FILE cmake/util/tsan_ignorelist.txt ABSOLUTE)

    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
  endif()
endif()
