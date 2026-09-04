# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(generated_uml "${CMAKE_CURRENT_BINARY_DIR}/diagram.puml")
sen_generate_uml(
  TARGET cmake_generated_uml
  OUT ${generated_uml}
  STL_FILES stl/diagram.stl
  CLASSES_ONLY
)

# sen_generate_uml() creates an on-demand target, so make it part of the test_package build.
add_custom_target(cmake_generated_uml_fixture ALL)
add_dependencies(cmake_generated_uml_fixture cmake_generated_uml)
add_generated_file_test(
  cmake.sen_generate_uml.diagram
  "${generated_uml}"
  REQUIRED_TEXT
  "DiagramClass"
  FORBIDDEN_TEXT
  "DiagramData"
)
