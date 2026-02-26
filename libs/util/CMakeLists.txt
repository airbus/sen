# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(dr_public_headers include/sen/util/dr/algorithms.h include/sen/util/dr/dead_reckoner.h
                      include/sen/util/dr/settable_dead_reckoner.h
)

set(dr_detail_headers
    include/sen/util/dr/detail/dead_reckoner_base.h include/sen/util/dr/detail/dead_reckoner_impl.h
    include/sen/util/dr/detail/settable_dead_reckoner_impl.h
)

set(dr_sources
    src/dr/algorithms.cpp
    src/dr/constants.h
    src/dr/dead_reckoner_impl.cpp
    src/dr/dead_reckoner_base.cpp
    src/dr/mat3.h
    src/dr/quat.h
    src/dr/settable_dead_reckoner_impl.cpp
    src/dr/utils.cpp
    src/dr/utils.h
    src/dr/vec3.h
)

# -------------------------------------------------------------------------------------------------------------
# library
# -------------------------------------------------------------------------------------------------------------

add_library(util SHARED ${dr_public_headers} ${dr_detail_headers} ${dr_sources})

add_library(sen::util ALIAS util)
sen_internal_configure_lib(util)
sen_enable_static_analysis(util)

if(WIN32)
  set_property(TARGET util PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()

target_include_directories(
  util
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs/util/include>"
  PUBLIC "$<INSTALL_INTERFACE:libs/util/include>"
  PRIVATE src
)

target_link_libraries(util PUBLIC sen::core)

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group("dr\\public headers" FILES ${dr_public_headers})
source_group("dr\\detail headers" FILES ${dr_detail_headers})
source_group("dr\\sources" FILES ${dr_sources})

# -------------------------------------------------------------------------------------------------------------
# install
# -------------------------------------------------------------------------------------------------------------

install(FILES ${dr_public_headers} DESTINATION libs/util/include/sen/util/dr)
install(FILES ${dr_detail_headers} DESTINATION libs/util/include/sen/util/dr/detail)

sen_internal_install(util)

# -------------------------------------------------------------------------------------------------------------
# testing
# -------------------------------------------------------------------------------------------------------------

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()
