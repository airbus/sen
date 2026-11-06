# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# The extension files beside this list add members to types the RPR FOM declares, one file per
# kind of member. They live here rather than in examples/packages/hla_fom/rpr, which stays
# exactly as published.
#
# No package is built from them. A second package over the same FOM directory would generate a
# second, different rpr model, and two components claiming the same class with different members
# is the type clash libs/kernel/test/integration/type_clash exists to catch. The diagram below is
# the artefact; that extensions reach generated C++ is held by libs/core/test/lang/fom_parser_test
# and by test/util/fom_extension, which does not compile if a contributed attribute is missing.

# --8<-- [start:uml]
sen_generate_uml(
  TARGET fom_extensions_uml
  OUT ${CMAKE_CURRENT_BINARY_DIR}/fom_extensions.plantuml
  HLA_FOM_DIRS
    ../hla_fom/rpr
    HLA_EXTENSIONS_FILE
    extensions/site_physical_entity.xml
    extensions/site_enumerations.xml
    extensions/site_records.xml
    extensions/site_variants.xml
    extensions/site_interaction.xml
  CLASSES_ONLY
)
# --8<-- [end:uml]
