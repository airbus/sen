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

# Skip configs whose required component are disabled. Detection is path-based. If the
# directory contains "recorder" -> recorder-dependent, etc.
# Explorer configs (_exp.yaml) need a display and are always skipped from smoke tests.
function(_sen_example_required_component config_file out_var)
  if(config_file MATCHES "/[0-9]+_recorder/")
    set(${out_var}
        "RECORDER"
        PARENT_SCOPE
    )
  elseif(config_file MATCHES "/[0-9]+_replayer/")
    set(${out_var}
        "REPLAYER"
        PARENT_SCOPE
    )
  elseif(config_file MATCHES "/[0-9]+_influx/")
    set(${out_var}
        "INFLUX"
        PARENT_SCOPE
    )
  elseif(config_file MATCHES "/[0-9]+_python/")
    set(${out_var}
        "PY"
        PARENT_SCOPE
    )
  else()
    set(${out_var}
        ""
        PARENT_SCOPE
    )
  endif()
endfunction()

# the shell is always used in our smoke tests
if(NOT SEN_BUILD_SHELL)
  return()
endif()

foreach(config_file ${configs})

  if(NOT
     config_file
     MATCHES
     "/[0-9]+_+.*\.yaml"
  )
    continue()
  endif()

  # explorer GUI configs need a display; never smoke-test them
  if(config_file MATCHES "_exp.yaml$")
    continue()
  endif()

  _sen_example_required_component(${config_file} required_component)
  if(required_component AND NOT SEN_BUILD_${required_component})
    continue()
  endif()

  get_filename_component(config ${config_file} NAME_WE)

  set(test_name ${config}_smoke)
  add_sen_run_smoke_test(${test_name} CONFIG_FILE ${config_file})

  append_test_env_modification(
    ${test_name} "PYTHONPATH=path_list_append:${PROJECT_SOURCE_DIR}/examples/config/10_python/scripts"
  )
endforeach()
