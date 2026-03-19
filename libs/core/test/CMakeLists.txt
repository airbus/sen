# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(base_tests
    base/assert_test.cpp
    base/bits_test.cpp
    base/duration_test.cpp
    base/hash32_test.cpp
    base/integer_compare_test.cpp
    base/iterator_adapters_test.cpp
    base/memory_block_test.cpp
    base/move_only_function_test.cpp
    base/mutex_utils_test.cpp
    base/quantity_test.cpp
    base/result_test.cpp
    base/scope_guard_test.cpp
    base/source_location_test.cpp
    base/span_test.cpp
    base/static_vector_test.cpp
    base/timestamp_test.cpp
)

set(meta_tests
    meta/alias_type_test.cpp
    meta/callable_test.cpp
    meta/class_type_test.cpp
    meta/enum_type_test.cpp
    meta/event_test.cpp
    meta/method_test.cpp
    meta/native_types_test.cpp
    meta/optional_type_test.cpp
    meta/property_test.cpp
    meta/quantity_type_test.cpp
    meta/sequence_type_test.cpp
    meta/struct_traits_test.cpp
    meta/struct_type_test.cpp
    meta/time_types_test.cpp
    meta/type_test.cpp
    meta/type_traits_test.cpp
    meta/type_visitor_test.cpp
    meta/unit_registry_test.cpp
    meta/unit_test.cpp
    meta/var_test.cpp
    meta/variant_type_test.cpp
)

set(io_tests
    io/endianness_test.cpp
    io/input_stream_test.cpp
    io/output_stream_test.cpp
    io/input_output_stream_test.cpp
    io/serialization_traits_test.cpp
    io/utils_io_test.cpp
    io/variant_adapter_test.cpp
    io/streaming_read_write.cpp
)

set(lang_tests lang/parser_test.cpp lang/compiler_test.cpp lang/resolver_test.cpp)

set(obj_tests
    obj/callback_test.cpp
    obj/interest_test.cpp
    obj/work_queue_test.cpp
    obj/event_buffer_test.cpp
    obj/property_flags_test.cpp
    obj/object_mux_test.cpp
)

add_library(core_test_io_generated OBJECT)
sen_generate_cpp(
  TARGET core_test_io_generated
  STL_FILES io/stl/test_io.stl
  BASE_PATH io
)

add_library(core_test_meta_generated OBJECT)
sen_generate_cpp(
  TARGET core_test_meta_generated
  STL_FILES meta/stl/test_type_traits.stl
  STL_FILES meta/stl/test_struct_traits.stl
  BASE_PATH meta
)

add_library(core_test_obj_generated OBJECT)
sen_generate_cpp(
  TARGET core_test_obj_generated
  STL_FILES obj/stl/example_class.stl
  BASE_PATH obj
)

add_sen_unit_test_suite(
  core_test
  ${base_tests}
  ${meta_tests}
  ${lang_tests}
  ${io_tests}
  ${obj_tests}
  LINK_DEPS
  sen::core
  PRIVATE
  cpptrace::cpptrace
  core_test_io_generated
  core_test_meta_generated
  core_test_obj_generated
)

set_target_properties(
  core_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

source_group("base" FILES ${base_tests})
source_group("meta" FILES ${meta_tests})
source_group("lang" FILES ${lang_tests})
source_group("io" FILES ${io_tests})
source_group("obj" FILES ${obj_tests})

target_compile_definitions(core_test PRIVATE ABS_FILE="${CMAKE_CURRENT_SOURCE_DIR}/base/hash32_test.cpp")
