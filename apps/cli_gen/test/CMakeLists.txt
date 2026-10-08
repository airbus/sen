# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(hla_fom_gen_test_sources ${CMAKE_CURRENT_SOURCE_DIR})
set(working_dir $<TARGET_FILE_DIR:sen::cli_gen>)

# Test case 1
#
# ModuleA using a type defined in ModuleB. Dependency declared.
#
# Expected result: success
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_1 DESCRIPTION
  "Generates code for a module that uses a type from a module it declares a dependency on."
  COMMAND ./cli_gen cpp fom --directories=${hla_fom_gen_test_sources}/test1/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 2
#
# ModuleA using a type defined in ModuleB. Dependency not declared.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_2 DESCRIPTION
  "Refuses a module that uses a type from a module it declares no dependency on."
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
  hla_fom_gen_smoke_3 DESCRIPTION "Accepts a type reached through a chain of declared dependencies."
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
  DESCRIPTION
  "Accepts a chain of declared dependencies whose modules sit in different folders, so in different namespaces."
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
  hla_fom_gen_smoke_5 DESCRIPTION "Refuses a transitive type when the first link of the chain is not declared."
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
  hla_fom_gen_smoke_6 DESCRIPTION "Refuses a transitive type when a later link of the chain is not declared."
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
  hla_fom_gen_smoke_7 DESCRIPTION
  "Maps an interaction onto a class in another module with no dependency declared between them."
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
  hla_fom_gen_smoke_8 DESCRIPTION
  "Refuses that mapping when the class also uses a type from the other module and declares no dependency."
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
  hla_fom_gen_smoke_9 DESCRIPTION
  "Accepts an interaction whose parameter type comes from a module it declares a dependency on."
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
  hla_fom_gen_smoke_10 DESCRIPTION
  "Accepts an interaction mapped as a method whose return value is another interaction."
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
  hla_fom_gen_smoke_11 DESCRIPTION "Maps interactions from two modules as events on one class."
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
  hla_fom_gen_smoke_12 DESCRIPTION
  "Accepts an interaction that inherits from one in a module it declares a dependency on."
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
  hla_fom_gen_smoke_13 DESCRIPTION
  "Refuses an interaction that inherits from one in a module it declares no dependency on."
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
  DESCRIPTION
  "Refuses an inherited interaction when the two modules share a third they both declare but do not declare each other."
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
  hla_fom_gen_smoke_15 DESCRIPTION
  "Accepts an inherited interaction when the two modules declare each other and the third they share."
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
  hla_fom_gen_smoke_16 DESCRIPTION
  "Refuses a class that inherits from one in a module it declares no dependency on."
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
  hla_fom_gen_smoke_17 DESCRIPTION
  "Accepts class inheritance across three modules, each declaring the one it inherits from."
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

set_target_properties(
  repeated_types_tester PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_sen_smoke_test(
  test_repeated_types_generation DESCRIPTION
  "Checks that two modules defining the same interaction name generate types suffixed by their module."
  COMMAND repeated_types_tester
  WORKING_DIRECTORY ${working_dir} REQ_DEPS repeated_types_tester
)

# Test case 19
#
# ModuleA-19 defines a class with an attribute that will then be implemented as
# checked using codegen settings
#
# Tests that the cli_gen reads codegen settings correctly for FOM code generation
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_sen_integration_test(
  hla_fom_gen_integration_test_19
  DESCRIPTION
  "Checks that the generator reads the codegen settings when generating FOM code."
  COMMAND
  ${Python3_EXECUTABLE}
  ${hla_fom_gen_test_sources}/test19/test.py
  WORKING_DIRECTORY ${working_dir}
  REQ_DEPS cli_gen
)

# link them so the checker only runs if the generator succeeds
set_tests_properties(hla_fom_gen_integration_test_19 PROPERTIES ENVIRONMENT "PYTHONUNBUFFERED=1")

# Test case 20
#
# ModuleA-20 defines interactions with optional and required parameters
#
# Tests that the cli_gen generates MaybeXX types for optional parameters and standard types for required parameters
add_sen_integration_test(
  hla_fom_gen_integration_test_20
  DESCRIPTION
  "Checks that an optional parameter generates a Maybe type and a required one a plain type."
  COMMAND
  ${Python3_EXECUTABLE}
  ${hla_fom_gen_test_sources}/test20/test.py
  WORKING_DIRECTORY ${working_dir}
  REQ_DEPS cli_gen
)

# link them so the checker only runs if the generator succeeds
set_tests_properties(hla_fom_gen_integration_test_20 PROPERTIES ENVIRONMENT "PYTHONUNBUFFERED=1")

# Test case 21
#
# ModuleA-21 defines a fixed record that includes the fields of another one.
#
# Check that after compilation the including record carries both its own field and the included one
add_sen_package(
  TARGET included_record_fom
  HLA_FOM_DIRS test21/fom
  GEN_HDR_FILES included_record_fom_headers
  PUBLIC_SYMBOLS
)

add_executable(included_record_tester test21/main.cpp)
sen_configure_target(included_record_tester)
target_link_libraries(included_record_tester PRIVATE sen::core included_record_fom)

set_target_properties(
  included_record_tester PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_sen_smoke_test(
  test_included_record_generation DESCRIPTION
  "Checks that a fixed record including another carries both its own field and the included one."
  COMMAND included_record_tester
  WORKING_DIRECTORY ${working_dir} REQ_DEPS included_record_tester
)

# Test case 22
#
# A fixed record including two other records, which a sen struct cannot represent.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_22 DESCRIPTION
  "Refuses a fixed record that includes two others, which a sen struct cannot represent."
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test22/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 23
#
# A fixed record including a type that is not a record.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_23 DESCRIPTION "Refuses a fixed record that includes a type that is not a record."
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test23/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 24
#
# A fixed record including itself.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_24 DESCRIPTION "Refuses a fixed record that includes itself."
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test24/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 25
#
# Two fixed records whose fields name each other.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_25 DESCRIPTION "Refuses two fixed records whose fields name each other."
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test25/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 26
#
# An array whose element type is itself.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_26 DESCRIPTION "Refuses an array whose element type is itself."
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test26/fom
  WORKING_DIRECTORY ${working_dir}
)

# Test case 27
#
# Two fixed records that include each other.
#
# Expected result: failure
add_sen_cli_gen_smoke_test(
  hla_fom_gen_smoke_27 DESCRIPTION "Refuses two fixed records that include each other."
  COMMAND ./cli_gen --expect-failure cpp fom --directories=${hla_fom_gen_test_sources}/test27/fom
  WORKING_DIRECTORY ${working_dir}
)

# JSON-Schema structure: one test renders the fixture and asserts the result by JSON path.
#
# This replaced thirteen PASS_REGULAR_EXPRESSION checks over `cat lossless.json`. They matched
# substrings in document order, so a file holding only `{"nonsense": "x-enum ... idle ... : 0"}`
# satisfied the enum check and three lines of prose satisfied the inheritance one. They also read
# an artefact a separate render test wrote into the shared binary directory, coupled by DEPENDS,
# which sequences tests but does not gate them. The checker renders and verifies in one process.
add_sen_integration_test(
  json_schema_structure
  DESCRIPTION
  "Renders the JSON-Schema fixture and asserts the result by JSON path."
  COMMAND
  ${Python3_EXECUTABLE}
  ${hla_fom_gen_test_sources}/json_schema_lossless/check_json_schema.py
  WORKING_DIRECTORY ${working_dir}
  REQ_DEPS cli_gen
)
set_tests_properties(json_schema_structure PROPERTIES ENVIRONMENT "PYTHONUNBUFFERED=1")

# TypeScript generator smoke tests.
add_sen_cli_gen_smoke_test(
  ts_gen_smoke_happy DESCRIPTION "Generates TypeScript from a FOM."
  COMMAND ./cli_gen ts stl ${hla_fom_gen_test_sources}/typescript/fixture.stl -d ts_smoke_happy
  WORKING_DIRECTORY ${working_dir}
)

# Failure path: omitting -d must error out, not silently produce nothing.
# CLI11's `->required()` rejects parsing before runApp; we gate on the non-zero exit
# via WILL_FAIL rather than the app's --expect-failure flag.
add_sen_cli_gen_smoke_test(
  ts_gen_smoke_missing_outdir DESCRIPTION "Refuses a TypeScript run that names no output directory."
  COMMAND ./cli_gen ts stl ${hla_fom_gen_test_sources}/typescript/fixture.stl
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(ts_gen_smoke_missing_outdir PROPERTIES WILL_FAIL TRUE)

# Structural check of the emitted TypeScript, replacing nine PASS_REGULAR_EXPRESSION checks that
# asked only whether a sentinel appeared somewhere in the file. All nine passed against a file
# reading `export type Direction = nonsense`. This compares whole declarations.
#
# It is the only test of the alias, enum and quantity templates: the client's tsc --noEmit covers
# the generated types under strict, but that is a different invocation over jsonrpc.stl, which
# declares none of those three.
add_sen_integration_test(
  ts_gen_structure
  DESCRIPTION
  "Asserts the structure of the emitted TypeScript, not that a sentinel appears somewhere in it."
  COMMAND
  ${Python3_EXECUTABLE}
  ${hla_fom_gen_test_sources}/typescript/check_typescript.py
  WORKING_DIRECTORY ${working_dir}
  REQ_DEPS cli_gen
)
set_tests_properties(ts_gen_structure PROPERTIES ENVIRONMENT "PYTHONUNBUFFERED=1")

# An unrecognised flag must be rejected rather than collected and dropped. These two run as a
# pair: the first fixes the invocation as valid, so the second can only fail because of the
# flag. WILL_FAIL on its own cannot tell a rejected flag from a missing file.
add_sen_cli_gen_smoke_test(
  uml_gen_smoke_only_classes DESCRIPTION
  "Generates UML for classes alone, fixing the invocation that the unknown-flag test varies."
  COMMAND ./cli_gen uml stl ${hla_fom_gen_test_sources}/typescript/fixture.stl -o uml_smoke_only_classes.uml
          --only-classes
  WORKING_DIRECTORY ${working_dir}
)

add_sen_cli_gen_smoke_test(
  uml_gen_smoke_rejects_unknown_flag DESCRIPTION
  "Refuses an unrecognised flag rather than collecting and dropping it."
  COMMAND ./cli_gen uml stl ${hla_fom_gen_test_sources}/typescript/fixture.stl -o uml_smoke_rejected.uml
          --classes-only
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(uml_gen_smoke_rejects_unknown_flag PROPERTIES WILL_FAIL TRUE)

# A second generator, because the flag was swallowed by whichever subcommand allowed extras and
# every generator set that separately.
add_sen_cli_gen_smoke_test(
  ts_gen_smoke_rejects_unknown_flag DESCRIPTION "Refuses an unrecognised flag in the TypeScript generator too."
  COMMAND ./cli_gen ts stl ${hla_fom_gen_test_sources}/typescript/fixture.stl -d ts_smoke_rejected
          --not-a-real-flag
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(ts_gen_smoke_rejects_unknown_flag PROPERTIES WILL_FAIL TRUE)

# The html generator writes an application rather than a file, so both input routes are worth
# their own checks. These assert the exit status only; what the five files contain is covered by
# the unit tests in libs/gen/test.
add_sen_cli_gen_smoke_test(
  html_gen_smoke_fom DESCRIPTION "Generates the HTML application from a FOM."
  COMMAND ./cli_gen html fom --directories=${hla_fom_gen_test_sources}/test1/fom -o html_smoke_fom
  WORKING_DIRECTORY ${working_dir}
)

# The stl route registers its own -o and -t and excludes the fom one; none of that is reached by
# the fom tests above.
add_sen_cli_gen_smoke_test(
  html_gen_smoke_stl DESCRIPTION "Generates the HTML application from an STL, which registers its own options."
  COMMAND ./cli_gen html stl ${hla_fom_gen_test_sources}/typescript/fixture.stl -o html_smoke_stl
  WORKING_DIRECTORY ${working_dir}
)

# CLI11's `->required()` rejects during parsing, before runApp sees the arguments, so these gate
# on the non-zero exit rather than on the app's --expect-failure flag.
add_sen_cli_gen_smoke_test(
  html_gen_smoke_missing_outdir DESCRIPTION "Refuses an HTML run that names no output directory."
  COMMAND ./cli_gen html fom --directories=${hla_fom_gen_test_sources}/test1/fom
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(html_gen_smoke_missing_outdir PROPERTIES WILL_FAIL TRUE)

# An unknown flag must be refused, as it is for the sibling generators since #272. A typo in a
# build script would otherwise generate the wrong thing in silence.
add_sen_cli_gen_smoke_test(
  html_gen_smoke_unknown_flag DESCRIPTION "Refuses an unrecognised flag in the HTML generator."
  COMMAND ./cli_gen html fom --directories=${hla_fom_gen_test_sources}/test1/fom -o html_smoke_flag --nosuch
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(html_gen_smoke_unknown_flag PROPERTIES WILL_FAIL TRUE)

# mkdocs without -o must fall back to output.md
add_sen_cli_gen_smoke_test(
  mkdocs_gen_smoke_default_output_stl DESCRIPTION
  "Generates mkdocs markdown from an STL model with no output flag, writing the default output file."
  COMMAND ./cli_gen mkdocs stl ${hla_fom_gen_test_sources}/typescript/fixture.stl
  WORKING_DIRECTORY ${working_dir}
)

add_sen_cli_gen_smoke_test(
  mkdocs_gen_smoke_default_output_fom DESCRIPTION
  "Generates mkdocs markdown from a FOM module with no output flag, writing the default output file."
  COMMAND ./cli_gen mkdocs fom --directories=${hla_fom_gen_test_sources}/test1/fom
  WORKING_DIRECTORY ${working_dir}
)

# The generators below had no test of their own: the suite above drives the C++ and HTML paths and
# leaves the rest of the front ends unread. Each takes the same type set and writes its own form of
# it, so a generator that stopped producing anything is caught here rather than by whoever next
# regenerates the documentation.
set(_stl_input ${CMAKE_SOURCE_DIR}/components/py/test/stl/py_test_package.stl)

add_sen_cli_gen_smoke_test(
  json_gen_component_schema DESCRIPTION
  "Generates the JSON schema for a component's configuration, which is what validates a config file."
  COMMAND ./cli_gen json component stl ${CMAKE_SOURCE_DIR}/components/influx/stl/configuration.stl -n influx -o
          json_gen_component.json
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(json_gen_component_schema PROPERTIES PASS_REGULAR_EXPRESSION "stl[|]json>")

add_sen_cli_gen_smoke_test(
  typst_gen_reference DESCRIPTION "Generates the typesettable reference for a package of types."
  COMMAND ./cli_gen typst stl ${_stl_input} -o typst_gen_reference
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(typst_gen_reference PROPERTIES PASS_REGULAR_EXPRESSION "stl[|]typst>")

add_sen_cli_gen_smoke_test(
  mkdocs_gen_reference DESCRIPTION "Generates the MkDocs pages for a package of types."
  COMMAND ./cli_gen mkdocs stl ${_stl_input} -o mkdocs_gen_reference
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(mkdocs_gen_reference PROPERTIES PASS_REGULAR_EXPRESSION "stl[|]mkdocs>")

add_sen_cli_gen_smoke_test(
  python_gen_dataclasses DESCRIPTION "Generates the Python dataclasses for a package of types."
  COMMAND ./cli_gen py stl ${_stl_input}
  WORKING_DIRECTORY ${working_dir}
)

# A generator that cannot read its input must say so rather than write an empty reference.
add_sen_cli_gen_smoke_test(
  typst_gen_refuses_a_missing_input DESCRIPTION "Refuses an STL file that is not there."
  COMMAND ./cli_gen typst stl ${CMAKE_CURRENT_BINARY_DIR}/no_such.stl -o typst_gen_missing
  WORKING_DIRECTORY ${working_dir}
)
set_tests_properties(typst_gen_refuses_a_missing_input PROPERTIES WILL_FAIL TRUE)
