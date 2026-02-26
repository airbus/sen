# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/json)
include(tps/spdlog)
include(tps/concurrentqueue)
include(tps/cpptrace)
include(tps/pugixml)

get_version_from_git_raw(SEN_VERSION_STRING)
configure_file(src/base/version.h.in ${CMAKE_CURRENT_LIST_DIR}/include/sen/core/base/version.h @ONLY)

# -------------------------------------------------------------------------------------------------------------
# base
# -------------------------------------------------------------------------------------------------------------

set(base_public_headers
    include/sen/core/base/assert.h
    include/sen/core/base/bits.h
    include/sen/core/base/checked_conversions.h
    include/sen/core/base/class_helpers.h
    include/sen/core/base/compiler_macros.h
    include/sen/core/base/duration.h
    include/sen/core/base/gradient_noise.h
    include/sen/core/base/hash32.h
    include/sen/core/base/integer_compare.h
    include/sen/core/base/iterator_adapters.h
    include/sen/core/base/memory_block.h
    include/sen/core/base/move_only_function.h
    include/sen/core/base/mutex_utils.h
    include/sen/core/base/numbers.h
    include/sen/core/base/quantity.h
    include/sen/core/base/result.h
    include/sen/core/base/scope_guard.h
    include/sen/core/base/source_location.h
    include/sen/core/base/span.h
    include/sen/core/base/static_vector.h
    include/sen/core/base/strong_type.h
    include/sen/core/base/timestamp.h
    include/sen/core/base/type_traits_extra.h
    include/sen/core/base/u8string_util.h
    include/sen/core/base/uuid.h
    include/sen/core/base/version.h
)

set(base_detail_headers include/sen/core/base/detail/assert_impl.h
                        include/sen/core/base/detail/move_only_function_impl.h
)

set(base_sources
    src/base/assert.cpp
    src/base/checked_conversions.cpp
    src/base/hash32.cpp
    src/base/result_impl.cpp
    src/base/uuid.cpp
    src/base/memory_block.cpp
    src/base/timestamp.cpp
)

# -------------------------------------------------------------------------------------------------------------
# obj
# -------------------------------------------------------------------------------------------------------------

set(obj_public_headers
    include/sen/core/obj/interest.h
    include/sen/core/obj/callback.h
    include/sen/core/obj/connection_guard.h
    include/sen/core/obj/object.h
    include/sen/core/obj/native_object.h
    include/sen/core/obj/object_provider.h
    include/sen/core/obj/object_mux.h
    include/sen/core/obj/object_filter.h
    include/sen/core/obj/object_list.h
    include/sen/core/obj/object_source.h
    include/sen/core/obj/subscription.h
    include/sen/core/obj/object_ref.h
)

set(obj_detail_headers
    include/sen/core/obj/detail/work_queue.h
    include/sen/core/obj/detail/native_object_proxy.h
    include/sen/core/obj/detail/native_object_impl.h
    include/sen/core/obj/detail/property_flags.h
    include/sen/core/obj/detail/proxy_object.h
    include/sen/core/obj/detail/remote_object.h
    include/sen/core/obj/detail/event_buffer.h
    include/sen/core/obj/detail/gen.h
)

set(obj_sources
    src/obj/connection_guard.cpp
    src/obj/object.cpp
    src/obj/object_filter.cpp
    src/obj/native_object.cpp
    src/obj/remote_object.cpp
    src/obj/work_queue.cpp
    src/obj/interest.cpp
    src/obj/object_mux.cpp
    src/obj/object_provider.cpp
    src/obj/native_object_proxy.cpp
    src/obj/callback.cpp
)

# -------------------------------------------------------------------------------------------------------------
# io
# -------------------------------------------------------------------------------------------------------------

set(io_public_headers
    include/sen/core/io/buffer_writer.h
    include/sen/core/io/input_stream.h
    include/sen/core/io/output_stream.h
    include/sen/core/io/buffer_writer.h
    include/sen/core/io/util.h
)

set(io_detail_headers include/sen/core/io/detail/endianness.h
                      include/sen/core/io/detail/serialization_traits.h
)

set(io_sources src/io/input_stream.cpp src/io/util.cpp)

# -------------------------------------------------------------------------------------------------------------
# meta
# -------------------------------------------------------------------------------------------------------------

set(meta_public_headers
    include/sen/core/meta/alias_type.h
    include/sen/core/meta/optional_type.h
    include/sen/core/meta/optional_traits.h
    include/sen/core/meta/callable.h
    include/sen/core/meta/class_type.h
    include/sen/core/meta/custom_type.h
    include/sen/core/meta/enum_type.h
    include/sen/core/meta/enum_traits.h
    include/sen/core/meta/event.h
    include/sen/core/meta/method.h
    include/sen/core/meta/native_types.h
    include/sen/core/meta/property.h
    include/sen/core/meta/quantity_type.h
    include/sen/core/meta/quantity_traits.h
    include/sen/core/meta/sequence_type.h
    include/sen/core/meta/sequence_traits.h
    include/sen/core/meta/struct_traits.h
    include/sen/core/meta/struct_type.h
    include/sen/core/meta/time_types.h
    include/sen/core/meta/type.h
    include/sen/core/meta/type_match.h
    include/sen/core/meta/type_traits.h
    include/sen/core/meta/type_utils.h
    include/sen/core/meta/basic_traits.h
    include/sen/core/meta/type_visitor.h
    include/sen/core/meta/type_registry.h
    include/sen/core/meta/unit.h
    include/sen/core/meta/unit_registry.h
    include/sen/core/meta/var.h
    include/sen/core/meta/variant_type.h
    include/sen/core/meta/variant_traits.h
)

set(meta_detail_headers include/sen/core/meta/detail/native_types_impl.h
                        include/sen/core/meta/detail/type_impl.h include/sen/core/meta/detail/types_fwd.h
)

set(meta_sources
    src/meta/alias_type.cpp
    src/meta/optional_type.cpp
    src/meta/callable.cpp
    src/meta/class_type.cpp
    src/meta/custom_type.cpp
    src/meta/enum_type.cpp
    src/meta/event.cpp
    src/meta/method.cpp
    src/meta/native_types.cpp
    src/meta/property.cpp
    src/meta/quantity_type.cpp
    src/meta/sequence_type.cpp
    src/meta/struct_type.cpp
    src/meta/type.cpp
    src/meta/type_match.cpp
    src/meta/type_visitor.cpp
    src/meta/type_registry.cpp
    src/meta/time_types.cpp
    src/meta/unit.cpp
    src/meta/unit_registry.cpp
    src/meta/utils.h
    src/meta/var.cpp
    src/meta/variant_type.cpp
    src/meta/variant_traits.cpp
)

# -------------------------------------------------------------------------------------------------------------
# lang
# -------------------------------------------------------------------------------------------------------------

set(lang_public_headers
    include/sen/core/lang/code_location.h
    include/sen/core/lang/error_reporter.h
    include/sen/core/lang/stl_expression.h
    include/sen/core/lang/stl_parser.h
    include/sen/core/lang/stl_resolver.h
    include/sen/core/lang/stl_scanner.h
    include/sen/core/lang/stl_statement.h
    include/sen/core/lang/vm.h
    include/sen/core/lang/stl_token.h
    include/sen/core/lang/fom_parser.h
)

set(lang_sources
    src/lang/error_reporter.cpp
    src/lang/stl_parser.cpp
    src/lang/stl_resolver.cpp
    src/lang/stl_scanner.cpp
    src/lang/vm.cpp
    src/lang/stl_token.cpp
    src/lang/fom_document_set.cpp
    src/lang/fom_document_set.h
    src/lang/fom_parser.cpp
)

# -------------------------------------------------------------------------------------------------------------
# library
# -------------------------------------------------------------------------------------------------------------

find_package(Threads REQUIRED)

add_library(
  core SHARED
  ${base_public_headers}
  ${base_detail_headers}
  ${base_sources}
  ${obj_public_headers}
  ${obj_detail_headers}
  ${obj_sources}
  ${meta_public_headers}
  ${meta_detail_headers}
  ${meta_sources}
  ${io_public_headers}
  ${io_detail_headers}
  ${io_sources}
  ${lang_public_headers}
  ${lang_sources}
)

add_library(sen::core ALIAS core)
sen_internal_configure_lib(core)
sen_enable_static_analysis(core)

if(WIN32)
  set_property(TARGET core PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()

target_include_directories(
  core
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs/core/include>"
  PUBLIC "$<INSTALL_INTERFACE:libs/core/include>"
  PRIVATE src
)

target_link_libraries(
  core
  PUBLIC ${CMAKE_THREAD_LIBS_INIT}
  PRIVATE nlohmann_json::nlohmann_json
  PRIVATE spdlog::spdlog
  PRIVATE concurrentqueue::concurrentqueue
  PRIVATE cpptrace::cpptrace
  PRIVATE pugixml::pugixml
)

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group("base\\public headers" FILES ${base_public_headers})
source_group("base\\detail headers" FILES ${base_detail_headers})
source_group("base\\sources" FILES ${base_sources})

source_group("obj\\public headers" FILES ${obj_public_headers})
source_group("obj\\detail headers" FILES ${obj_detail_headers})
source_group("obj\\sources" FILES ${obj_sources})

source_group("meta\\public headers" FILES ${meta_public_headers})
source_group("meta\\detail headers" FILES ${meta_detail_headers})
source_group("meta\\sources" FILES ${meta_sources})

source_group("io\\public headers" FILES ${io_public_headers})
source_group("io\\detail headers" FILES ${io_detail_headers})
source_group("io\\sources" FILES ${io_sources})

source_group("lang\\public headers" FILES ${lang_public_headers})
source_group("lang\\detail headers" FILES ${lang_detail_headers})
source_group("lang\\sources" FILES ${lang_sources})

# -------------------------------------------------------------------------------------------------------------
# install
# -------------------------------------------------------------------------------------------------------------

install(FILES ${base_public_headers} DESTINATION libs/core/include/sen/core/base)
install(FILES ${base_detail_headers} DESTINATION libs/core/include/sen/core/base/detail)

install(FILES ${obj_public_headers} DESTINATION libs/core/include/sen/core/obj)
install(FILES ${obj_detail_headers} DESTINATION libs/core/include/sen/core/obj/detail)

install(FILES ${meta_public_headers} DESTINATION libs/core/include/sen/core/meta)
install(FILES ${meta_detail_headers} DESTINATION libs/core/include/sen/core/meta/detail)

install(FILES ${io_public_headers} DESTINATION libs/core/include/sen/core/io)
install(FILES ${io_detail_headers} DESTINATION libs/core/include/sen/core/io/detail)

install(FILES ${lang_public_headers} DESTINATION libs/core/include/sen/core/lang)
install(FILES ${lang_detail_headers} DESTINATION libs/core/include/sen/core/lang/detail)

sen_internal_install(core)

# -------------------------------------------------------------------------------------------------------------
# testing
# -------------------------------------------------------------------------------------------------------------

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()
