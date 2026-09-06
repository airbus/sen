# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(schema_files)

set(packages
    animals
    calculators
    school
    hla_fom
    aircrafts
    terrain_server
    weather_server
    shapes
    fibonacci
    timer
    my_counter
)

# add the subdirectories and fetch the schemas
foreach(package ${packages})
  add_subdirectory(${package})

  if(TARGET ${package})
    get_target_property(schema ${package} SCHEMA)
    if(schema)
      list(APPEND schema_files ${schema})
    endif()
  endif()
endforeach()

# the CMake usage of Makefiles has a problem, so we work-around it
if(NOT
   "${CMAKE_GENERATOR}"
   STREQUAL
   "Unix Makefiles"
)

  # add the sen schemas
  file(GLOB sen_schemas ${SEN_PATH}/schemas/*.json)
  list(APPEND schema_files ${sen_schemas})

  # assemble the final schema
  set(final_schema_file ${PROJECT_SOURCE_DIR}/examples/config/base/schema.json)
  sen_combine_schemas(OUTPUT ${final_schema_file} SCHEMAS ${schema_files})
  add_custom_target(examples_schema ALL DEPENDS ${final_schema_file})

endif()
