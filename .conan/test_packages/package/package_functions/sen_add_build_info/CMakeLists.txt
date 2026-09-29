# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# A component built with plain add_library rather than add_sen_package. component.h only declares the
# provenance accessors, so such a target has to ask for the generated file that defines them.
# Linking is the assertion: without that call this does not link.
add_library(hand_rolled_provenance SHARED src/provenance.cpp)
sen_configure_target(hand_rolled_provenance)
target_link_libraries(hand_rolled_provenance PUBLIC sen::kernel)
sen_add_build_info(hand_rolled_provenance hand_rolled_provenance)
