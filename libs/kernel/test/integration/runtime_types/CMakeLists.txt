# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# A component that knows rpr.Aircraft only from a FOM it imports at runtime.
add_sen_package(
  TARGET runtime_types_subscriber
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Subscribes to a class it imported from a FOM at runtime"
  SOURCES src/subscriber.cpp
  IS_COMPONENT NO_SCHEMA
)
sen_enable_static_analysis(runtime_types_subscriber)
set_target_properties(runtime_types_subscriber PROPERTIES FOLDER "test")

# Two processes: one publishes dummy_entities' generated aircraft, the other receives it with the
# component above, as a GenericRemoteObject.
add_sen_integration_test(
  kernel_runtime_types_remote_fallback
  DESCRIPTION
  "Receives a generated rpr.Aircraft in a process that knows the class only from importing its FOM at runtime."
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/subscriber.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_COMPONENTS ether REQ_DEPS runtime_types_subscriber dummy_entities
)

# The FOM and mappings that dummy_fom is generated from, for the runtime import.
append_test_env_modification(
  kernel_runtime_types_remote_fallback
  "SEN_RUNTIME_TYPES_FOM_DIR=set:${PROJECT_SOURCE_DIR}/test/util/dummy_entities/hla/rpr"
  "SEN_RUNTIME_TYPES_MAPPINGS_FILE=set:${PROJECT_SOURCE_DIR}/test/util/dummy_entities/hla/mapping.xml"
)
set_tests_properties(kernel_runtime_types_remote_fallback PROPERTIES RUN_SERIAL TRUE)
