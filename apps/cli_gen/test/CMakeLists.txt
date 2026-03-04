# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(hla_fom_gen_test_sources ${CMAKE_CURRENT_SOURCE_DIR})
set(working_dir ${CMAKE_BINARY_DIR}/bin)

# Test case 1
#
# ModuleA using a type defined in ModuleB. Dependency declared.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_1
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test1/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 2
#
# ModuleA using a type defined in ModuleB. Dependency not declared.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_2
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test2/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 3
#
# ModuleA using type defined in ModuleC.
# ModuleA declares a dependency with ModuleB, which declares a dependency with ModuleC.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_3
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test3/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 4
#
# ModuleA using type defined in ModuleC. ModuleA declares a dependency with ModuleB, which declares a dependency with ModuleC.
# Modules coming from different folders (different namespaces in c++)
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_4
  COMMAND
    ./cli_gen cpp fom
    --directories=${hla_fom_gen_test_sources}/test4/fom/moduleA;${hla_fom_gen_test_sources}/test4/fom/moduleB;${hla_fom_gen_test_sources}/test4/fom/moduleC
  WORKING_DIRECTORY ${working_dir}
)

# Test case 5
#
# ModuleA using type defined in ModuleC.
# ModuleA doesn't declare a dependency with ModuleB, which declares a dependency with ModuleC.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_5
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test5/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 6
#
# ModuleA using type defined in ModuleC.
# ModuleA declares a dependency with ModuleB, which doesn't declare a dependency with ModuleC.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_6
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test6/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 7
#
# ModuleB interaction mapped to ModuleA class.
# There are no dependencies declared between the modules.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_7
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test7/fom
          --mappings=${hla_fom_gen_test_sources}/test7/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 8
#
# ModuleB interaction mapped to ModuleA class.
# ModuleA using type defined in ModuleB.
# There are no dependencies declared between the modules.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_8
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test8/fom
          --mappings=${hla_fom_gen_test_sources}/test8/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 9
#
# ModuleB interaction mapped to ModuleA class.
# ModuleB interaction uses type defined in ModuleC.
# ModuleB declares a dependency with ModuleC.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_9
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test9/fom
          --mappings=${hla_fom_gen_test_sources}/test9/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 10
#
# ModuleB interaction with other ModuleB interaction as return mapped as method to ModuleA class.
# ModuleB interactions use type defined in ModuleC.
# ModuleB declares a dependency with ModuleC.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_10
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test10/fom
          --mappings=${hla_fom_gen_test_sources}/test10/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 11
#
# ModuleB and ModuleC interactions mapped as events to ModuleA class.
# There are no dependencies declared between the modules.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_11
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test11/fom
          --mappings=${hla_fom_gen_test_sources}/test11/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 12
#
# ModuleB interaction inherits from ModuleC interaction.
# ModuleB interaction is mapped to ModuleA class.
# ModuleB declares a dependency with ModuleC.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_12
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test12/fom
          --mappings=${hla_fom_gen_test_sources}/test12/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 13
#
# ModuleB interaction inherits from ModuleC interaction.
# ModuleB interaction is mapped to ModuleA class.
# There are no dependencies declared between the modules.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_13
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test13/fom
          --mappings=${hla_fom_gen_test_sources}/test13/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 14
#
# ModuleB interaction inherits from ModuleC interaction.
# ModuleB interaction is mapped to ModuleA class.
# ModuleB and ModuleC use ModuleD type and declare dependency with ModuleD.
# ModuleB doesn't declare dependency with ModuleC
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_14
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test14/fom
          --mappings=${hla_fom_gen_test_sources}/test14/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 15
#
# ModuleB interaction inherits from ModuleC interaction.
# ModuleB interaction is mapped to ModuleA class.
# ModuleB and ModuleC use ModuleD type and declare dependency with ModuleD.
# ModuleB declares dependency with ModuleC
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_15
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test15/fom
          --mappings=${hla_fom_gen_test_sources}/test15/mapping/mapping.xml
  WORKING_DIRECTORY ${working_dir}
)

# Test case 16
#
# ModuleB class inherits from ModuleA class.
# ModuleB doesn't declare dependency with ModuleA.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_16
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test16/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 17
#
# ModuleB class inherits from ModuleA class. ModuleB declares dependency with ModuleA.
# ModuleC class inherits from ModuleB class. ModuleC declares dependency with ModuleB.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_17
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test17/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 18
#
# Module-A and Module-B both define a RepeatedInteraction.
#
# Check that after compilation, the generated types add the suffix of the modules:
# Resulting types: RepeatedInteractionA and RepeatedInteractionB
add_sen_package(
  TARGET repeated_fom
  HLA_FOM_DIRS test18/fom
  GEN_HDR_FILES repeated_fom_headers
  PUBLIC_SYMBOLS
)

add_executable(repeated_types_tester test18/main.cpp)
sen_configure_target(repeated_types_tester)
target_link_libraries(repeated_types_tester PRIVATE sen::core repeated_fom)

add_sen_smoke_test(
  test_repeated_types_generation
  COMMAND repeated_types_tester
  WORKING_DIRECTORY ${working_dir} REQ_DEPS repeated_types_tester
)
