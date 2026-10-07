# === expect_output.cmake ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Runs a command and requires a string in what it printed, whatever its exit status.
#
# ctest can do this with PASS_REGULAR_EXPRESSION, but not when the process ends on a signal: a
# component that reports a bad configuration is still brought down abruptly by the kernel's
# teardown, and ctest reads that as "Subprocess aborted" rather than as a failure the test can
# expect. What this checks is the component's half: that the mistake was named. The exit status is
# deliberately not asserted, so a test here does not have to be rewritten when the teardown is.

execute_process(
  COMMAND "${EXECUTABLE}" "${CONFIG}"
  TIMEOUT 60
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _out
  ERROR_VARIABLE _err
)

if(NOT
   "${_out}${_err}"
   MATCHES
   "${EXPECT}"
)
  message(STATUS "${_out}")
  message(STATUS "${_err}")
  message(FATAL_ERROR "the output did not mention '${EXPECT}' (the command exited ${_result})")
endif()
