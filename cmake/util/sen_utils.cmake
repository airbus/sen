# === sen_utils.cmake ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

include(util/git_info)
include(CheckCXXCompilerFlag)

# cmake miscellaneous utils used in sen
include(util/sen_misc_utils)

# utils used for code generation
include(util/sen_codegen_utils)

# utils used to create Sen packages
include(util/sen_package_utils)
