# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)
include(tps/concurrentqueue)
include(tps/lz4)

set(db_stl_files stl/sen/db/basic_types.stl)

set(db_format_stl_files src/v1.stl)

set(public_headers
    include/sen/db/output.h
    include/sen/db/input.h
    include/sen/db/cursor.h
    include/sen/db/creation.h
    include/sen/db/deletion.h
    include/sen/db/event.h
    include/sen/db/keyframe.h
    include/sen/db/snapshot.h
    include/sen/db/annotation.h
    include/sen/db/property_change.h
)

set(sources
    src/output.cpp
    src/input.cpp
    src/annotation.cpp
    src/deletion.cpp
    src/creation.cpp
    src/event.cpp
    src/snapshot.cpp
    src/keyframe.cpp
    src/property_change.cpp
    src/util.h
    src/util.cpp
    src/constants.h
)

# -------------------------------------------------------------------------------------------------------------
# library
# -------------------------------------------------------------------------------------------------------------

add_library(
  db SHARED
  ${public_headers}
  ${sources}
  ${db_stl_files}
  ${db_format_stl_files}
)

add_library(sen::db ALIAS db)
sen_internal_configure_lib(db)
sen_enable_static_analysis(db)

if(WIN32)
  set_property(TARGET db PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()

sen_generate_cpp(
  TARGET db
  STL_FILES ${db_stl_files}
  GEN_HDR_FILES public_generated_files
)

sen_generate_cpp(
  TARGET db
  STL_FILES ${db_format_stl_files}
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# adding sen::kernel as a dependency (it has STL-generated headers)
sen_add_dependency(db PUBLIC sen::kernel)

target_include_directories(
  db
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  PUBLIC "$<INSTALL_INTERFACE:libs/db/include>"
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
  db
  PUBLIC sen::core
  PUBLIC ${CMAKE_DL_LIBS}
  PUBLIC ${CMAKE_THREAD_LIBS_INIT}
  PRIVATE spdlog::spdlog
  PRIVATE concurrentqueue::concurrentqueue
  PRIVATE LZ4::lz4_static
)

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group("public headers" FILES ${public_headers})
source_group("src" FILES ${sources})
source_group("stl" FILES ${db_stl_files} ${db_format_stl_files})

# -------------------------------------------------------------------------------------------------------------
# install
# -------------------------------------------------------------------------------------------------------------

install(FILES ${public_headers} DESTINATION libs/db/include/sen/db)
install(FILES ${public_generated_files} DESTINATION libs/db/include/stl/sen/db)
install(FILES ${db_stl_files} DESTINATION interfaces/stl/sen/db)

sen_internal_install(db)

# -------------------------------------------------------------------------------------------------------------
# testing
# -------------------------------------------------------------------------------------------------------------

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()
