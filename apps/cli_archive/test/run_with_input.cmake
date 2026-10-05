# === run_with_input.cmake =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Runs a command with a file on its standard input. ctest cannot do that on its own, and the merge
# command asks at the terminal how to resolve a duplicate object, so a test that does not answer
# hangs. Separate arguments are passed as ARG0..ARGn because a list cannot survive -D.

set(_command "${EXECUTABLE}")
foreach(_index RANGE 15)
  if(DEFINED ARG${_index})
    list(APPEND _command "${ARG${_index}}")
  endif()
endforeach()

execute_process(
  COMMAND ${_command}
  INPUT_FILE "${INPUT_FILE}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err
)

message(STATUS "${_out}")
message(STATUS "${_err}")

if(NOT
   _result
   EQUAL
   0
)
  message(FATAL_ERROR "the command exited ${_result}")
endif()

if(DEFINED EXPECT
   AND NOT
       "${_out}"
       MATCHES
       "${EXPECT}"
)
  message(FATAL_ERROR "the output did not match '${EXPECT}'")
endif()
