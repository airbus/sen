# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_subdirectory(unit)

# Disable integration tests in Windows
if(NOT MSVC)
  add_subdirectory(integration/test_helpers)
  # These drive several `sen run` processes through runner.py; they need no
  # container, only the ether and py components.
  add_subdirectory(integration/transport)
  add_subdirectory(integration/runtime_compatibility)
  add_subdirectory(integration/crash_report)
  add_subdirectory(integration/type_clash)
  add_subdirectory(integration/termination)
  #
  # integration tests that use containers (only available in Linux)
  if(LINUX)
    # The container-based suites run the built binaries inside docker containers and
    # need a runtime image. Opt in by configuring with
    # -DSEN_INTEGRATION_TEST_IMAGE=<image> (CI does; see tools/ci/runtime.Dockerfile).
    # Without the image the tests are not registered, so a plain local ctest run
    # stays green on machines without docker. Both suites drive the ether component,
    # so neither can be registered when the build leaves it out.
    if(SEN_INTEGRATION_TEST_IMAGE AND SEN_BUILD_ETHER)
      add_subdirectory(integration/object_sync)
    endif()
    if(SEN_BUILD_ETHER)
      add_subdirectory(integration/interest_filtering)
    endif()
    #   add_subdirectory(integration/stress_tests/high_churn_subs)
  endif()
endif()
