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

# Walk yaml_path's top-level `load:` block and (recursively) the files referenced by its
# `include:` block, collecting each `- name: <X>` at the load list's outer indent. Names
# at deeper indents (e.g. shell.query items) are skipped. Cycle-safe via visited set.
function(
  _sen_collect_load_components
  yaml_path
  out_var
  visited_var
)
  get_filename_component(_abs "${yaml_path}" ABSOLUTE)
  if(_abs IN_LIST ${visited_var})
    set(${out_var}
        ""
        PARENT_SCOPE
    )
    return()
  endif()
  list(APPEND ${visited_var} "${_abs}")
  set(${visited_var}
      "${${visited_var}}"
      PARENT_SCOPE
  )

  if(NOT EXISTS "${_abs}")
    message(FATAL_ERROR "examples/config: included file not found: ${_abs}")
  endif()

  file(STRINGS "${_abs}" _lines)
  set(_section "")
  set(_collected "")
  set(_includes "")
  foreach(_line IN LISTS _lines)
    if(_line MATCHES "^load:")
      set(_section "load")
    elseif(_line MATCHES "^include:")
      set(_section "include")
    elseif(_line MATCHES "^[A-Za-z]")
      set(_section "")
    elseif(_section STREQUAL "load" AND _line MATCHES "^  - name: ([A-Za-z_][A-Za-z0-9_]*)")
      list(APPEND _collected "${CMAKE_MATCH_1}")
    elseif(_section STREQUAL "include" AND _line MATCHES "^  - (.+)$")
      set(_inc "${CMAKE_MATCH_1}")
      string(STRIP "${_inc}" _inc)
      list(APPEND _includes "${_inc}")
    endif()
  endforeach()

  get_filename_component(_yaml_dir "${_abs}" DIRECTORY)
  foreach(_inc IN LISTS _includes)
    get_filename_component(_inc_abs "${_yaml_dir}/${_inc}" ABSOLUTE)
    _sen_collect_load_components("${_inc_abs}" _sub ${visited_var})
    list(APPEND _collected ${_sub})
    set(${visited_var}
        "${${visited_var}}"
        PARENT_SCOPE
    )
  endforeach()

  set(${out_var}
      "${_collected}"
      PARENT_SCOPE
  )
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

  set(_visited "")
  _sen_collect_load_components("${config_file}" _required_components _visited)
  list(REMOVE_DUPLICATES _required_components)

  set(_skip_smoke FALSE)
  foreach(_comp IN LISTS _required_components)
    string(TOUPPER "${_comp}" _comp_upper)
    if(NOT DEFINED SEN_BUILD_${_comp_upper})
      message(
        FATAL_ERROR
          "examples/config: '${config_file}' loads component '${_comp}', but "
          "SEN_BUILD_${_comp_upper} is not defined. Either it is a typo or a "
          "new Sen component is missing from conanfile.py COMPONENTS."
      )
    endif()
    if(NOT SEN_BUILD_${_comp_upper})
      set(_skip_smoke TRUE)
      break()
    endif()
  endforeach()
  if(_skip_smoke)
    continue()
  endif()

  get_filename_component(config ${config_file} NAME_WE)

  set(test_name ${config}_smoke)
  add_sen_run_smoke_test(${test_name} CONFIG_FILE ${config_file})

  append_test_env_modification(
    ${test_name} "PYTHONPATH=path_list_append:${PROJECT_SOURCE_DIR}/examples/config/10_python/scripts"
  )
endforeach()
