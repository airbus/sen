# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/pybind11)
include(tps/spdlog)

find_package(Python3 REQUIRED COMPONENTS Development)

if(Python3_Development_FOUND)

  sen_generate_package(
    TARGET py_interface
    STL_FILES stl/configuration.stl stl/python_interpreter.stl
    NO_SCHEMA
  )

  sen_internal_install(py_interface)

  add_sen_component(
    TARGET py
    MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
    VERSION "0.0.1"
    DESCRIPTION "Python interpreter inside Sen"
    SOURCES src/component.cpp
            src/component_api.h
            src/component_api.cpp
            src/variant_conversion.h
            src/variant_conversion.cpp
            src/object_source_wrapper.h
            src/object_source_wrapper.cpp
            src/object_list_wrapper.h
            src/object_list_wrapper.cpp
            src/object_wrapper.h
            src/object_wrapper.cpp
            src/interpreter_obj.h
            src/interpreter_obj.cpp
    DEPS py_interface Python3::Python pybind11::pybind11
    PRIVATE_DEPS spdlog::spdlog
  )

  set_target_properties(py PROPERTIES FOLDER "components/py")

  target_link_libraries(
    py
    PUBLIC pybind11::pybind11
    PUBLIC Python3::Python
    PRIVATE $<BUILD_INTERFACE:spdlog::spdlog>
  )

  sen_internal_install(py)

endif()
