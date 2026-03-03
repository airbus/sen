# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET hla_fom
  BASE_PATH ${CMAKE_CURRENT_LIST_DIR}
  HLA_OUTPUT_DIR hla_fom
  HLA_FOM_DIRS rpr netn link16
  GEN_HDR_FILES fom_headers
  HLA_MAPPINGS_FILE base_mappings.xml
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
  PUBLIC_SYMBOLS
)

sen_generate_uml(
  TARGET hla_fom_uml
  OUT ${CMAKE_CURRENT_BINARY_DIR}/hla_fom.plantuml
  HLA_FOM_DIRS rpr netn link16
  CLASSES_ONLY
)
