# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# find all the configuration files
file(
  GLOB_RECURSE
  configs
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.yaml"
)

# create a smoke test for each configuratin
foreach(config_file ${configs})

  # skip explorer-related configurations
  if(config_file MATCHES "/[0-9]+_+.*\.yaml")
    if(NOT
       config_file
       MATCHES
       "_exp.yaml$"
    )

      get_filename_component(config ${config_file} NAME_WE)

      set(test_name ${config}_smoke)
      add_sen_run_smoke_test(${test_name} CONFIG_FILE ${config_file})

      append_test_env_modification(
        ${test_name} "PYTHONPATH=path_list_append:${PROJECT_SOURCE_DIR}/examples/config/10_python/scripts"
      )
    endif()
  endif()
endforeach()
