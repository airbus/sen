# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# What this covers is the component loading, not the tracing being right. Every method of TracyTracer is
# a one-line Tracy macro whose effect is only observable in a profiler, and a changed Tracer interface
# fails the build rather than a test, because the class is final and every override says so.
#
# What it does catch is load-time breakage -- a missing symbol, a bad export, a component that cannot be
# dlopened by name -- which no other test here would see, since nothing else loads tracy at all.
add_sen_run_smoke_test(
  tracy_smoke DESCRIPTION "Loads the tracy component by name, which nothing else in the tree does."
  CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/tracy.yaml
)
