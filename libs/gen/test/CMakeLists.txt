# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(gen_tests
    json_generator_test.cpp
    html_generator_test.cpp
    typst_generator_test.cpp
    generators_test.cpp
    plantuml_fixture_test.h
    plantuml_stl_generator_test.cpp
    plantuml_fom_generator_test.cpp
    every_kind_model.h
)

add_sen_unit_test_suite(
  gen_test
  ${gen_tests}
  LINK_DEPS
  sen::core
  sen::gen
  nlohmann_json::nlohmann_json
)

set_target_properties(
  gen_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

target_compile_definitions(gen_test PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
