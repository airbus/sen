# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/implot)
include(tps/spdlog)

if(APPLE)
  return()
endif()

# get the imgui root dir
get_target_property(IMGUI_INCLUDE_DIR_WITH_GEN imgui::imgui INTERFACE_INCLUDE_DIRECTORIES)
string(
  REGEX
  REPLACE "\\$<\\$<CONFIG:[^>]+>:(.*)>"
          "\\1"
          IMGUI_INCLUDE_DIR
          "${IMGUI_INCLUDE_DIR_WITH_GEN}"
)
set(IMGUI_ROOT_DIR "${IMGUI_INCLUDE_DIR}/..")

set(common_sources ${IMGUI_ROOT_DIR}/res/misc/cpp/imgui_stdlib.cpp)

if(WIN32)
  set(backend_sources src/backends/win32_dx11.cpp ${IMGUI_ROOT_DIR}/res/bindings/imgui_impl_dx11.cpp
                      ${IMGUI_ROOT_DIR}/res/bindings/imgui_impl_win32.cpp
  )

  set(backend_libs d3d11 d3dcompiler)
else()
  include(tps/sdl2)

  # avoid compiling the explorer if SDL2 is not found
  if(SDL2_FOUND)

    # get interface link libraries
    get_target_property(SDL2_INTERFACE_LINK SDL2::SDL2 INTERFACE_LINK_LIBRARIES)
    string(
      REGEX MATCH
            "CONAN_LIB::[^>]+"
            SDL2_CONAN2_TARGET
            "${SDL2_INTERFACE_LINK}"
    )

    # get .so location
    string(TOUPPER ${CMAKE_BUILD_TYPE} UPPER_CMAKE_BUILD_TYPE)
    get_target_property(SDL2_LIB ${SDL2_CONAN2_TARGET} IMPORTED_LOCATION_${UPPER_CMAKE_BUILD_TYPE})

    # copy sdl2 shared object to our bin path
    set(sdl_libs_folder ${CMAKE_BINARY_DIR}/sdl_libs)
    file(
      COPY "${SDL2_LIB}"
      DESTINATION ${sdl_libs_folder}
      FOLLOW_SYMLINK_CHAIN
    )

    # collect all the shared objects and the symbolic links
    file(GLOB sdl_files ${sdl_libs_folder}/*)

    # install the shared objects in our binary dir
    install(FILES ${sdl_files} DESTINATION ${CMAKE_INSTALL_BINDIR})
  else()
    message("SDL2 not found. Disabling the explorer component.")
    return()
  endif()

  find_package(OpenGL COMPONENTS OpenGL)

  if(OPENGL_FOUND)
    set(backend_sources src/backends/sdl2.cpp ${IMGUI_ROOT_DIR}/res/bindings/imgui_impl_sdl2.cpp
                        ${IMGUI_ROOT_DIR}/res/bindings/imgui_impl_opengl3.cpp
    )

    set(backend_libs SDL2::SDL2 OpenGL::GL)
  else()
    message(STATUS "Did not find OpenGL - *NOT* building the explorer component")
    return()
  endif()
endif()

add_library(explorer_imgui STATIC ${common_sources} ${backend_sources})

target_include_directories(
  explorer_imgui
  PRIVATE ${IMGUI_ROOT_DIR}/res/bindings
  PUBLIC ${IMGUI_ROOT_DIR}/res/misc/cpp
)

target_link_libraries(
  explorer_imgui
  PUBLIC imgui::imgui
         implot::implot
         ${backend_libs}
         core
)

add_sen_package(
  TARGET explorer
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Sen GUI frontend"
  SOURCES src/bus_window.h
          src/bus_window.cpp
          src/component.cpp
          src/main_bar.h
          src/main_bar.cpp
          src/event_explorer.h
          src/event_explorer.cpp
          src/circular_list.h
          src/property_data.h
          src/property_data.cpp
          src/object_state.h
          src/object_state.cpp
          src/value.h
          src/icons_font.h
          src/icons_font.cpp
          src/value_font.h
          src/value_font.cpp
          src/value.cpp
          src/plotter.h
          src/plotter.cpp
          src/quantity_visitor.h
          src/quantity_visitor.cpp
          src/editable_printer_maker.h
          src/editable_printer_maker.cpp
          src/method_printer.h
          src/method_printer.cpp
          src/util.h
          src/util.cpp
  STL_FILES stl/configuration.stl
  PRIVATE_DEPS explorer_imgui spdlog::spdlog
  IS_COMPONENT
)

set_target_properties(explorer PROPERTIES FOLDER "components/explorer")

sen_internal_install(explorer_imgui)
sen_internal_install(explorer)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/explorer.json DESTINATION schemas)

# Install small helper script to make it user-friendly to start the sen explorer
# Modify the helper script path to point towards the installation directory given to CPack
install(
  CODE "configure_file(${CMAKE_CURRENT_LIST_DIR}/pkg/sen-explorer.in ${CMAKE_CURRENT_BINARY_DIR}/sen-explorer)"
  COMPONENT rpm
  EXCLUDE_FROM_ALL
)

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/sen-explorer
  DESTINATION /usr/bin
  COMPONENT rpm
  EXCLUDE_FROM_ALL
)

# Provide a desktop entry & Icon so you can start it from your desktop
# and also see the icon for the running application
install(
  FILES ${CMAKE_CURRENT_LIST_DIR}/pkg/sen-explorer.desktop
  DESTINATION /usr/share/applications
  COMPONENT rpm
  EXCLUDE_FROM_ALL
)

install(
  FILES ${PROJECT_SOURCE_DIR}/docs/assets/images/logo.svg
  DESTINATION /usr/share/icons
  RENAME sen.svg
  COMPONENT rpm
  EXCLUDE_FROM_ALL
)
