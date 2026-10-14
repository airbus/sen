# === spdlog.cmake =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

if(TARGET spdlog::spdlog)
  # spdlog's own CMake sets these PUBLIC when it builds shared; its conan recipe does not.
  target_compile_definitions(spdlog::spdlog INTERFACE SPDLOG_SHARED_LIB)
  if(MSVC)
    target_compile_options(spdlog::spdlog INTERFACE /wd4251 /wd4275)
  endif()

  get_target_property(SPDLOG_INCLUDES spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
  string(
    REGEX
    REPLACE "\\$<\\$<CONFIG:[^>]+>:(.*)>"
            "\\1"
            SPDLOG_INCLUDES
            "${SPDLOG_INCLUDES}"
  )

  # fmt's conan recipe names the target fmt::fmt-header-only when it ships no library, which is how
  # Sen requires it. Either name carries the include directory.
  if(TARGET fmt::fmt-header-only)
    set(_fmt_target fmt::fmt-header-only)
  else()
    set(_fmt_target fmt::fmt)
  endif()

  get_target_property(FMT_INCLUDES ${_fmt_target} INTERFACE_INCLUDE_DIRECTORIES)
  string(
    REGEX
    REPLACE "\\$<\\$<CONFIG:[^>]+>:(.*)>"
            "\\1"
            FMT_INCLUDES
            "${FMT_INCLUDES}"
  )

  if(SPDLOG_INCLUDES AND FMT_INCLUDES)
    install(DIRECTORY "${SPDLOG_INCLUDES}/spdlog" DESTINATION include)
    install(DIRECTORY "${FMT_INCLUDES}/fmt" DESTINATION include)

    # The library as well as the headers, so a consumer links the copy Sen ships rather than
    # compiling its own. The wildcard before the extension covers the `d` spdlog appends in Debug on
    # every platform; a release-only pattern matches nothing and installs nothing.
    get_filename_component(_spdlog_package_dir "${SPDLOG_INCLUDES}/.." ABSOLUTE)

    # The build tree carries it too, beside Sen's own libraries. Integration tests run Sen's
    # binaries in containers that mount the workspace and not the conan cache, where the build-tree
    # rpath points. On Windows a DLL is found beside the executable.
    if(WIN32)
      file(GLOB _spdlog_runtime "${_spdlog_package_dir}/bin/spdlog*.dll")
      set(_spdlog_runtime_dir "${PROJECT_BINARY_DIR}/bin")
    else()
      file(
        GLOB
        _spdlog_runtime
        "${_spdlog_package_dir}/lib/libspdlog*.so*"
        "${_spdlog_package_dir}/lib/libspdlog*.dylib*"
      )
      set(_spdlog_runtime_dir "${PROJECT_BINARY_DIR}/lib")
    endif()
    if(_spdlog_runtime)
      # Compared by content, and a copy that matches is left alone: the glob re-check re-runs this
      # file after a test run, and on Windows the copy in bin/ is a dll a binary there has loaded.
      # Default permissions when it does write, because conan marks its cache read-only.
      foreach(_spdlog_file IN LISTS _spdlog_runtime)
        get_filename_component(_spdlog_name "${_spdlog_file}" NAME)
        set(_spdlog_dest "${_spdlog_runtime_dir}/${_spdlog_name}")
        set(_spdlog_dest_hash "")
        if(EXISTS "${_spdlog_dest}")
          file(SHA256 "${_spdlog_dest}" _spdlog_dest_hash)
        endif()
        file(SHA256 "${_spdlog_file}" _spdlog_source_hash)
        if(NOT
           _spdlog_source_hash
           STREQUAL
           _spdlog_dest_hash
        )
          if(EXISTS "${_spdlog_dest}")
            message(STATUS "spdlog: replacing ${_spdlog_dest}")
          endif()
          file(
            COPY "${_spdlog_file}"
            DESTINATION "${_spdlog_runtime_dir}"
            NO_SOURCE_PERMISSIONS
          )
        endif()
      endforeach()
      # test.cmake copies these beside each test executable: a test binary lives in its own
      # directory and gtest_discover_tests runs it at build time, where no PATH reaches it.
      set(SEN_SPDLOG_RUNTIME
          "${_spdlog_runtime}"
          CACHE INTERNAL "spdlog's runtime files, to sit beside a binary on Windows"
      )
    endif()
    if(WIN32)
      install(
        DIRECTORY "${_spdlog_package_dir}/lib/"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILES_MATCHING
        PATTERN "spdlog*.lib"
      )
      install(
        DIRECTORY "${_spdlog_package_dir}/bin/"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILES_MATCHING
        PATTERN "spdlog*.dll"
      )
    else()
      install(
        DIRECTORY "${_spdlog_package_dir}/lib/"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILES_MATCHING
        PATTERN "libspdlog*.so*"
        PATTERN "libspdlog*.dylib*"
      )
    endif()
  endif()
endif()
