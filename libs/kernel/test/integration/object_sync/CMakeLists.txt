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
  PRIVATE_DEPS spdlog::spdlog test_helpers
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

set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)
set(_script_file ${CMAKE_CURRENT_SOURCE_DIR}/../run.py)

set(LISTENER_TYPE ListenerStaticProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/1_static_props)

add_sen_integration_test(
  object_sync_static_props_local_test
  DESCRIPTION
  "Carries static properties from a publisher to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_static_props_single_process_test
  DESCRIPTION
  "Carries static properties from a publisher to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_static_props_two_processes
  DESCRIPTION
  "Carries static properties from a publisher to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/1_static_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerBestEffortProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props)

add_sen_integration_test(
  object_sync_best_effort_props_local
  DESCRIPTION
  "Carries best-effort properties from a publisher to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_best_effort_props_single_process
  DESCRIPTION
  "Carries best-effort properties from a publisher to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_best_effort_props_two_processes
  DESCRIPTION
  "Carries best-effort properties from a publisher to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/2_best_effort_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerConfirmedProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props)

add_sen_integration_test(
  object_sync_confirmed_props_local
  DESCRIPTION
  "Carries confirmed properties from a publisher to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_confirmed_props_single_process
  DESCRIPTION
  "Carries confirmed properties from a publisher to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_confirmed_props_two_processes
  DESCRIPTION
  "Carries confirmed properties from a publisher to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/3_confirmed_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerMulticastProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props)

add_sen_integration_test(
  object_sync_multicast_props_local
  DESCRIPTION
  "Carries multicast properties from a publisher to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_multicast_props_single_process
  DESCRIPTION
  "Carries multicast properties from a publisher to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_multicast_props_two_processes
  DESCRIPTION
  "Carries multicast properties from a publisher to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/4_multicast_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerWritableProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/5_writable_props)

add_sen_integration_test(
  object_sync_writable_props_local
  DESCRIPTION
  "Carries writable properties from a publisher to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_writable_props_single_process
  DESCRIPTION
  "Carries writable properties from a publisher to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_writable_props_two_processes
  DESCRIPTION
  "Carries writable properties from a publisher to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/5_writable_props/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerBestEffortEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events)

add_sen_integration_test(
  object_sync_best_effort_events_local
  DESCRIPTION
  "Delivers best-effort events to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_best_effort_events_single_process
  DESCRIPTION
  "Delivers best-effort events to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_best_effort_events_two_processes
  DESCRIPTION
  "Delivers best-effort events to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/6_best_effort_events/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerConfirmedEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events)

add_sen_integration_test(
  object_sync_confirmed_events_local
  DESCRIPTION
  "Delivers confirmed events to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_confirmed_events_single_process
  DESCRIPTION
  "Delivers confirmed events to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_confirmed_events_two_processes
  DESCRIPTION
  "Delivers confirmed events to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/7_confirmed_events/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerMulticastEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events)

add_sen_integration_test(
  object_sync_multicast_events_local
  DESCRIPTION
  "Delivers multicast events to a listener within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_multicast_events_single_process
  DESCRIPTION
  "Delivers multicast events to a listener between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_multicast_events_two_processes
  DESCRIPTION
  "Delivers multicast events to a listener between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/8_multicast_events/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerLocalMethod)
configure_file(
  config/single_component.yaml.in ${CMAKE_CURRENT_BINARY_DIR}/9_local_method/single_component.yaml @ONLY
)

add_sen_integration_test(
  object_sync_local_method
  DESCRIPTION
  "Calls a method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/9_local_method/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerConstMethod)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/10_const_method)

add_sen_integration_test(
  object_sync_const_method_local
  DESCRIPTION
  "Calls a const method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_const_method_single_process
  DESCRIPTION
  "Calls a const method on a published object between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_const_method_two_processes
  DESCRIPTION
  "Calls a const method on a published object between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/10_const_method/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerConfirmedMethod)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/11_confirmed_method)

add_sen_integration_test(
  object_sync_confirmed_method_local
  DESCRIPTION
  "Calls a confirmed method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/11_confirmed_method/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_confirmed_method_single_process
  DESCRIPTION
  "Calls a confirmed method on a published object between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/11_confirmed_method/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_confirmed_method_two_processes
  DESCRIPTION
  "Calls a confirmed method on a published object between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/11_confirmed_method/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/11_confirmed_method/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(LISTENER_TYPE ListenerBestEffortMethod)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/12_best_effort_method)

add_sen_integration_test(
  object_sync_best_effort_method_local
  DESCRIPTION
  "Calls a best-effort method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/12_best_effort_method/single_component.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_best_effort_method_single_process
  DESCRIPTION
  "Calls a best-effort method on a published object between two components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/12_best_effort_method/single_process.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_best_effort_method_two_processes
  DESCRIPTION
  "Calls a best-effort method on a published object between two processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/12_best_effort_method/publisher.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/12_best_effort_method/listener.yaml
  REQ_DEPS
    object_sync py ether
    kernel
    FLAKY # marked as flaky (sanitizer tickets have been created and the test can fail around once every 100 runs)
    USE_TESTCONTAINERS
)

set(NODE_TYPE NodeStaticProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_1_static_props)

add_sen_integration_test(
  object_sync_p2p_static_props_local
  DESCRIPTION
  "Carries static properties within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_1_static_props/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_static_props_single_process
  DESCRIPTION
  "Carries static properties between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_1_static_props/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_static_props_multi_process
  DESCRIPTION
  "Carries static properties between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_1_static_props/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_1_static_props/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_1_static_props/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeBestEffortProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_2_best_effort_props)

add_sen_integration_test(
  object_sync_p2p_best_effort_props_local
  DESCRIPTION
  "Carries best-effort properties within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_2_best_effort_props/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_best_effort_props_single_process
  DESCRIPTION
  "Carries best-effort properties between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_2_best_effort_props/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_best_effort_props_multi_process
  DESCRIPTION
  "Carries best-effort properties between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_2_best_effort_props/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_2_best_effort_props/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_2_best_effort_props/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeConfirmedProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_3_confirmed_props)

add_sen_integration_test(
  object_sync_p2p_confirmed_props_local
  DESCRIPTION
  "Carries confirmed properties within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_3_confirmed_props/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_confirmed_props_single_process
  DESCRIPTION
  "Carries confirmed properties between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_3_confirmed_props/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_confirmed_props_multi_process
  DESCRIPTION
  "Carries confirmed properties between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_3_confirmed_props/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_3_confirmed_props/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_3_confirmed_props/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeMulticastProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_4_multicast_props)

add_sen_integration_test(
  object_sync_p2p_multicast_props_local
  DESCRIPTION
  "Carries multicast properties within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_4_multicast_props/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_multicast_props_single_process
  DESCRIPTION
  "Carries multicast properties between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_4_multicast_props/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_multicast_props_multi_process
  DESCRIPTION
  "Carries multicast properties between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_4_multicast_props/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_4_multicast_props/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_4_multicast_props/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeWritableProps)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_5_writable_props)

add_sen_integration_test(
  object_sync_p2p_writable_props_local
  DESCRIPTION
  "Carries writable properties within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_5_writable_props/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_writable_props_single_process
  DESCRIPTION
  "Carries writable properties between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_5_writable_props/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_writable_props_multi_process
  DESCRIPTION
  "Carries writable properties between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_5_writable_props/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_5_writable_props/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_5_writable_props/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeBestEffortEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_6_best_effort_events)

add_sen_integration_test(
  object_sync_p2p_best_effort_events_local
  DESCRIPTION
  "Delivers best-effort events within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_6_best_effort_events/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_best_effort_events_single_process
  DESCRIPTION
  "Delivers best-effort events between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_6_best_effort_events/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_best_effort_events_multi_process
  DESCRIPTION
  "Delivers best-effort events between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_6_best_effort_events/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_6_best_effort_events/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_6_best_effort_events/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeConfirmedEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_7_confirmed_events)

add_sen_integration_test(
  object_sync_p2p_confirmed_events_local
  DESCRIPTION
  "Delivers confirmed events within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_7_confirmed_events/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_confirmed_events_single_process
  DESCRIPTION
  "Delivers confirmed events between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_7_confirmed_events/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_confirmed_events_multi_process
  DESCRIPTION
  "Delivers confirmed events between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_7_confirmed_events/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_7_confirmed_events/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_7_confirmed_events/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeMulticastEvent)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_8_multicast_events)

add_sen_integration_test(
  object_sync_p2p_multicast_events_local
  DESCRIPTION
  "Delivers multicast events within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_8_multicast_events/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_multicast_events_single_process
  DESCRIPTION
  "Delivers multicast events between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_8_multicast_events/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_multicast_events_multi_process
  DESCRIPTION
  "Delivers multicast events between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_8_multicast_events/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_8_multicast_events/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_8_multicast_events/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeLocalMethod)
configure_file(
  config/p2p_single_component.yaml.in ${CMAKE_CURRENT_BINARY_DIR}/p2p_9_local_method/p2p_single_component.yaml
  @ONLY
)

add_sen_integration_test(
  object_sync_p2p_local_method_local
  DESCRIPTION
  "Calls a method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_9_local_method/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeConstMethod)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_10_const_method)

add_sen_integration_test(
  object_sync_p2p_const_method_local
  DESCRIPTION
  "Calls a const method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_10_const_method/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_const_method_single_process
  DESCRIPTION
  "Calls a const method on a published object between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_10_const_method/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_const_method_multi_process
  DESCRIPTION
  "Calls a const method on a published object between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_10_const_method/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_10_const_method/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_10_const_method/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeConfirmedMethod)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_11_confirmed_method)

add_sen_integration_test(
  object_sync_p2p_confirmed_method_local
  DESCRIPTION
  "Calls a confirmed method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_11_confirmed_method/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_confirmed_method_single_process
  DESCRIPTION
  "Calls a confirmed method on a published object between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_11_confirmed_method/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_confirmed_method_multi_process
  DESCRIPTION
  "Calls a confirmed method on a published object between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_11_confirmed_method/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_11_confirmed_method/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_11_confirmed_method/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

set(NODE_TYPE NodeBestEffortMethod)
generate_config_files(${CMAKE_CURRENT_BINARY_DIR}/p2p_12_best_effort_method)

add_sen_integration_test(
  object_sync_p2p_best_effort_method_local
  DESCRIPTION
  "Calls a best-effort method on a published object within one component."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_12_best_effort_method/p2p_single_component.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_best_effort_method_single_process
  DESCRIPTION
  "Calls a best-effort method on a published object between multiple components in one process."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_12_best_effort_method/p2p_single_process.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)

add_sen_integration_test(
  object_sync_p2p_best_effort_method_multi_process
  DESCRIPTION
  "Calls a best-effort method on a published object between multiple processes."
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_12_best_effort_method/p2p_multi_process_1.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_12_best_effort_method/p2p_multi_process_2.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/p2p_12_best_effort_method/p2p_multi_process_3.yaml
  REQ_DEPS object_sync py ether kernel FLAKY USE_TESTCONTAINERS
)
