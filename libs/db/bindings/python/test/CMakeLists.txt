# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Not on a sanitizer build: the test imports the bindings from a stock python3, which
# dlopens a sanitized libcore.so without the sanitizer runtime the executable would
# normally carry, so the import fails on an unresolved symbol before a single
# assertion runs. Running it there needs the runtime LD_PRELOADed, and that path is
# compiler- and architecture-specific.
if(SEN_USE_SANITIZER STREQUAL None)
  # Recording fixture lives alongside the recorder test data; the bindings test reuses it
  # rather than spinning up a Sen runtime to regenerate one per-build.
  set(_fixture ${PROJECT_SOURCE_DIR}/components/recorder/test/data/crash_recording)

  add_sen_integration_test(
    db_python_bindings_test
    COMMAND
    ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bindings.py
    ${_fixture}
    REQ_DEPS sen_db_python
  )

  # The test imports sen_db_python at runtime; point PYTHONPATH at the built .so directory.
  set_property(
    TEST db_python_bindings_test
    APPEND
    PROPERTY ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:sen_db_python>"
  )
endif()
