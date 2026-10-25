# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# A component that knows rpr.Aircraft only from a FOM it imports at runtime, and publishes one as a
# GenericLocalObject.
add_sen_package(
  TARGET runtime_fom_publisher
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Publishes an object of a class it imported from a FOM at runtime"
  SOURCES src/publisher.cpp
  IS_COMPONENT NO_SCHEMA
)
sen_enable_static_analysis(runtime_fom_publisher)
set_target_properties(runtime_fom_publisher PROPERTIES FOLDER "test")

# A component with dummy_fom's generated rpr.Aircraft. Linked directly rather than through DEPS:
# test/util is configured after libs/, and DEPS needs the target to exist already.
add_sen_package(
  TARGET generated_fom_subscriber
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Subscribes to a class through its generated code"
  SOURCES src/subscriber.cpp
  IS_COMPONENT NO_SCHEMA
)
target_link_libraries(generated_fom_subscriber PRIVATE dummy_fom)
sen_enable_static_analysis(generated_fom_subscriber)
set_target_properties(generated_fom_subscriber PROPERTIES FOLDER "test")

# Two processes: one publishes a GenericLocalObject of the runtime class, the other receives it
# through generated code.
add_sen_integration_test(
  kernel_runtime_publisher_generated_subscriber
  DESCRIPTION
  "Reads through generated code an rpr.Aircraft published as a GenericLocalObject of a class imported at runtime."
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/subscriber.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_COMPONENTS ether REQ_DEPS runtime_fom_publisher generated_fom_subscriber
)

# The FOM and mappings that dummy_fom is generated from, for the runtime import.
append_test_env_modification(
  kernel_runtime_publisher_generated_subscriber
  "SEN_RUNTIME_PUBLISHER_FOM_DIR=set:${PROJECT_SOURCE_DIR}/test/util/dummy_entities/hla/rpr"
  "SEN_RUNTIME_PUBLISHER_MAPPINGS_FILE=set:${PROJECT_SOURCE_DIR}/test/util/dummy_entities/hla/mapping.xml"
)
set_tests_properties(kernel_runtime_publisher_generated_subscriber PROPERTIES RUN_SERIAL TRUE)
