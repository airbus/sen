# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET object_sync
  STL_FILES stl/object_sync.stl
  SOURCES src/object_sync.cpp
  NO_SCHEMA
  PRIVATE_DEPS spdlog::spdlog
)

find_package(Python REQUIRED COMPONENTS Interpreter)

function(generate_config_files DESTINATION)
  file(
    GLOB
    templates
    CONFIGURE_DEPENDS
    config/*.in
  )

  set(template_names "")
  foreach(full_path ${templates})
    get_filename_component(name_only ${full_path} NAME)
    list(APPEND template_names ${name_only})
  endforeach()

  foreach(file ${template_names})
    string(
      REPLACE ".in"
              ""
              output_file
              ${file}
    )
    configure_file(config/${file} ${DESTINATION}/${output_file} @ONLY)
  endforeach()
endfunction()

set(RYUK_IMAGE "docker-proxy.pforgeipt-docker.intra.airbusds.corp/testcontainers/ryuk:0.8.1")

set(COMMON_TEST_ENV
    "PYTHONUNBUFFERED=set:1;RYUK_CONTAINER_IMAGE=set:${RYUK_IMAGE};ASAN_OPTIONS=set:suppressions=/home/builder/sen/cmake/util/asan_ignorelist.txt:fast_unwind_on_malloc=0:malloc_context_size=100;TSAN_OPTIONS=set:suppressions=/home/builder/sen/cmake/util/tsan_ignorelist.txt;LSAN_OPTIONS=set:suppressions=/home/builder/sen/cmake/util/lsan_ignorelist.txt:report_objects=1;UBSAN_OPTIONS=set:suppressions=/home/builder/sen/cmake/util/ubsan_ignorelist.txt"
)

set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)

set(LISTENER_TYPE ListenerStaticProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/1_static_props)

add_sen_integration_test(
  object_sync_static_props_local_test
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_static_props_local_test ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_static_props_single_process_test
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_static_props_single_process_test ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_static_props_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_static_props_two_processes ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerBestEffortProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props)

add_sen_integration_test(
  object_sync_best_effort_props_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_best_effort_props_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_best_effort_props_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_best_effort_props_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_best_effort_props_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_best_effort_props_two_processes ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerConfirmedProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props)

add_sen_integration_test(
  object_sync_confirmed_props_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_confirmed_props_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_confirmed_props_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_confirmed_props_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_confirmed_props_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_confirmed_props_two_processes ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerMulticastProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props)

add_sen_integration_test(
  object_sync_multicast_props_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_multicast_props_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_multicast_props_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_multicast_props_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_multicast_props_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_multicast_props_two_processes ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerWritableProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/5_writable_props)

add_sen_integration_test(
  object_sync_writable_props_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_writable_props_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_writable_props_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_writable_props_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_writable_props_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_writable_props_two_processes ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerBestEffortEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events)

add_sen_integration_test(
  object_sync_best_effort_events_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_best_effort_events_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_best_effort_events_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_best_effort_events_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_best_effort_events_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_best_effort_events_two_processes ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerConfirmedEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events)

add_sen_integration_test(
  object_sync_confirmed_events_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_confirmed_events_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_confirmed_events_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_confirmed_events_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_confirmed_events_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_confirmed_events_two_processes ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerMulticastEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events)

add_sen_integration_test(
  object_sync_multicast_events_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_multicast_events_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_multicast_events_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_multicast_events_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_multicast_events_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_multicast_events_two_processes ${COMMON_TEST_ENV})

configure_file(
  config/single_component.yaml.in ${CMAKE_CURRENT_BINARY_DIR}/9_local_method/single_component.yaml @ONLY
)

add_sen_integration_test(
  object_sync_local_method
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/9_local_method/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_local_method ${COMMON_TEST_ENV})

set(LISTENER_TYPE ListenerConstMethod)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/10_const_method)

add_sen_integration_test(
  object_sync_const_method_local
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_const_method_local ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_const_method_single_process
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_const_method_single_process ${COMMON_TEST_ENV})

add_sen_integration_test(
  object_sync_const_method_two_processes
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
)

# link them so the checker only runs if the generator succeeds
append_test_env_modification(object_sync_const_method_two_processes ${COMMON_TEST_ENV})
