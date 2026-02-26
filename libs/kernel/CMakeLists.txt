# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/rapidyaml)
include(tps/spdlog)
include(tps/concurrentqueue)
include(tps/cpptrace)

set(kernel_stl_files
    stl/sen/kernel/log.stl
    stl/sen/kernel/basic_types.stl
    stl/sen/kernel/kernel_objects.stl
    stl/sen/kernel/type_specs.stl
)

set(kernel_bus_stl_files src/bus/bus_protocol.stl)

set(public_headers
    include/sen/kernel/kernel.h
    include/sen/kernel/kernel_config.h
    include/sen/kernel/bootloader.h
    include/sen/kernel/component.h
    include/sen/kernel/component_api.h
    include/sen/kernel/package_manager.h
    include/sen/kernel/transport.h
    include/sen/kernel/tracer.h
    include/sen/kernel/util.h
    include/sen/kernel/source_info.h
    include/sen/kernel/type_specs_utils.h
    include/sen/kernel/test_kernel.h
)

set(detail_headers include/sen/kernel/detail/kernel_fwd.h)

set(sources
    src/bootloader.cpp
    src/component_api_impl.cpp
    src/executor.h
    src/executor.cpp
    src/crash_reporter.h
    src/crash_reporter.cpp
    src/kernel_impl.h
    src/kernel_impl.cpp
    src/kernel.cpp
    src/kernel_component.h
    src/kernel_component.cpp
    src/kernel_config.cpp
    src/package_manager.cpp
    src/operating_system.h
    src/pipeline_component.h
    src/pipeline_component.cpp
    src/plugin_manager.h
    src/plugin_manager.cpp
    src/precision_sleeper.h
    src/wall_clock.cpp
    src/spdlog_config.h
    src/spdlog_config.cpp
    src/runner.h
    src/runner.cpp
    src/util.cpp
    src/shared_library.h
    src/source_info.cpp
    src/thread.h
    src/tracer.cpp
    src/wall_clock.h
    src/transport.cpp
    src/test_kernel.cpp
    src/type_specs_utils.cpp
)

if(WIN32)
  set(os_sources
      src/win32/thread_impl.h
      src/win32/thread_impl.cpp
      src/win32/win32_api.cpp
      src/win32/win32_api.h
      src/win32/win32_os.cpp
      src/win32/win32_os.h
  )
else()
  set(os_sources
      src/posix/posix_api.cpp
      src/posix/posix_api.h
      src/posix/posix_os.cpp
      src/posix/posix_os.h
      src/posix/thread_impl.cpp
      src/posix/thread_impl.h
  )
endif()

set(bus_sources
    src/bus/bus.h
    src/bus/bus.cpp
    src/bus/participant.h
    src/bus/proxy_manager.h
    src/bus/proxy_manager.cpp
    src/bus/local_participant.h
    src/bus/local_participant.cpp
    src/bus/remote_interest_manager.h
    src/bus/remote_interest_manager.cpp
    src/bus/remote_participant.h
    src/bus/remote_participant.cpp
    src/bus/generic_remote_object.h
    src/bus/generic_remote_object.cpp
    src/bus/util.h
    src/bus/session.h
    src/bus/session.cpp
    src/bus/session_manager.h
    src/bus/session_manager.cpp
    src/bus/remote_interest_handler.h
    src/bus/containers.h
)

# -------------------------------------------------------------------------------------------------------------
# library
# -------------------------------------------------------------------------------------------------------------

add_library(
  kernel SHARED
  ${public_headers}
  ${detail_headers}
  ${sources}
  ${os_sources}
  ${kernel_stl_files}
  ${bus_sources}
)

sen_generate_cpp(
  TARGET kernel
  STL_FILES ${kernel_stl_files}
  GEN_HDR_FILES public_generated_files
)

sen_generate_cpp(
  TARGET kernel
  STL_FILES ${kernel_bus_stl_files}
  BASE_PATH src/bus
)

add_library(sen::kernel ALIAS kernel)
sen_internal_configure_lib(kernel)
sen_enable_static_analysis(kernel)

if(WIN32)
  set_property(TARGET kernel PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()

get_git_head_revision(git_ref_spec git_hash ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)
git_local_changes(git_status_str)

set_target_properties(kernel PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_compile_definitions(
  kernel
  PUBLIC SEN_KERNEL_VERSION="0.0.1"
  PRIVATE SEN_COMPILER_STRING="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
  PRIVATE GIT_REF_SPEC="${git_ref_spec}"
  PRIVATE GIT_HASH="${git_hash}"
  PRIVATE GIT_STATUS="${git_status_str}"
)

target_include_directories(
  kernel
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PUBLIC $<INSTALL_INTERFACE:libs/kernel/include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
  kernel
  PUBLIC sen::core
  PUBLIC ${CMAKE_DL_LIBS}
  PUBLIC ${CMAKE_THREAD_LIBS_INIT}
  PRIVATE ryml::ryml
  PRIVATE spdlog::spdlog
  PRIVATE concurrentqueue::concurrentqueue
  PRIVATE cpptrace::cpptrace
)

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group("public headers" FILES ${public_headers})
source_group("detail headers" FILES ${detail_headers})
source_group("src" FILES ${sources})
source_group("src/os" FILES ${os_sources})
source_group("src/bus" FILES ${bus_sources})
source_group("stl" FILES ${kernel_stl_files})
source_group("src/bus/sen" FILES ${kernel_bus_generated})

# -------------------------------------------------------------------------------------------------------------
# install
# -------------------------------------------------------------------------------------------------------------

install(FILES ${public_headers} DESTINATION libs/kernel/include/sen/kernel)
install(FILES ${public_generated_files} DESTINATION libs/kernel/include/stl/sen/kernel)
install(FILES ${detail_headers} DESTINATION libs/kernel/include/sen/kernel/detail)

sen_internal_install(kernel)

# -------------------------------------------------------------------------------------------------------------
# testing
# -------------------------------------------------------------------------------------------------------------

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()
