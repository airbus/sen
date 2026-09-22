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

set(generated_uml_all "${CMAKE_CURRENT_BINARY_DIR}/diagram_all.puml")
sen_generate_uml(
  TARGET cmake_generated_uml_all
  OUT ${generated_uml_all}
  STL_FILES stl/diagram.stl
)
add_custom_target(cmake_generated_uml_all_fixture ALL)
add_dependencies(cmake_generated_uml_all_fixture cmake_generated_uml_all)
add_generated_file_test(
  cmake.sen_generate_uml.diagram_all
  "${generated_uml_all}"
  REQUIRED_TEXT
  "DiagramData"
)

add_generated_file_test(
  cmake.generated_file.reject_missing_text
  "${generated_uml}"
  REQUIRED_TEXT
  "DiagramData"
)

add_generated_file_test(
  cmake.generated_file.reject_forbidden_text
  "${generated_uml_all}"
  FORBIDDEN_TEXT
  "DiagramData"
)
set_tests_properties(
  cmake.generated_file.reject_missing_text cmake.generated_file.reject_forbidden_text PROPERTIES WILL_FAIL
                                                                                                 TRUE
)
