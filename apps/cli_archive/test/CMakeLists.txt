# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_integration_test(
  cli_archive_info
  COMMAND
  ./cli_archive
  info
  ${CMAKE_CURRENT_SOURCE_DIR}/data/archive_recording
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_archive>
  REQ_DEPS cli_archive
)

add_sen_integration_test(
  cli_archive_indexed
  COMMAND
  ./cli_archive
  indexed
  ${CMAKE_CURRENT_SOURCE_DIR}/data/archive_recording
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_archive>
  REQ_DEPS cli_archive
)
