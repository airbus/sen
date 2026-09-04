# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(generated_yaml "${CMAKE_CURRENT_BINARY_DIR}/generated_config.yaml")
sen_generate_yaml(
  TARGET cmake_generated_yaml
  SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/generate_config.py
  OUTPUT ${generated_yaml}
  DEPS cmake_generated_python
)

# The build verifies that DEPS exposes the generated Python module through PYTHONPATH.
add_custom_target(cmake_generated_yaml_fixture ALL)
add_dependencies(cmake_generated_yaml_fixture cmake_generated_yaml)
add_generated_file_test(
  cmake.sen_generate_yaml.output
  "${generated_yaml}"
  REQUIRED_TEXT
  "generated-by-sen_generate_yaml"
)
