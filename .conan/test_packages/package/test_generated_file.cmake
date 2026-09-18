# === test_generated_file.cmake ========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

if(NOT DEFINED GENERATED_FILE OR NOT EXISTS "${GENERATED_FILE}")
  message(FATAL_ERROR "Expected generated file does not exist: ${GENERATED_FILE}")
endif()

file(SIZE "${GENERATED_FILE}" generated_file_size)
if(generated_file_size EQUAL 0)
  message(FATAL_ERROR "Generated file is empty: ${GENERATED_FILE}")
endif()

if(REQUIRED_TEXT)
  file(READ "${GENERATED_FILE}" generated_file_contents)
  string(FIND "${generated_file_contents}" "${REQUIRED_TEXT}" required_text_index)
  if(required_text_index EQUAL -1)
    message(FATAL_ERROR "Generated file does not contain '${REQUIRED_TEXT}': ${GENERATED_FILE}")
  endif()
endif()
