# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# example packages
add_subdirectory(packages)
add_subdirectory(apps)

# configurations
# A subdirectory rather than an include: the configs register their own tests, and CMake books a
# test against the directory it was registered from. Included here, all of them were attributed to
# examples/ rather than to examples/config, and the guard at the top of that file returned from
# this scope rather than its own.
add_subdirectory(config)
