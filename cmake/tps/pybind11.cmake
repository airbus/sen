# === pybind11.cmake ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

# Find Python here so that pybind11 does not do it on its own and possibly
# creates a version mismatch.
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

# Left to itself, pybind11 looks for Python through FindPythonLibs, which CMake
# 3.27 deprecated (CMP0148) and will remove. PYBIND11_FINDPYTHON sends it to
# FindPython instead, and the hint keeps it on the interpreter found above.
set(PYBIND11_FINDPYTHON ON)
set(Python_EXECUTABLE "${Python3_EXECUTABLE}")

find_package(pybind11 REQUIRED)
