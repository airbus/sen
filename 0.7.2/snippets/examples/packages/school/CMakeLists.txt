# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET school
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Allows the creation of class rooms"
  SOURCES src/names.h
          src/names.cpp
          src/person.h
          src/teacher.h
          src/teacher.cpp
          src/student.h
          src/student.cpp
          src/classroom.cpp
  STL_FILES stl/school/person.stl
            stl/school/student.stl
            stl/school/teacher.stl
            stl/school/classroom.stl
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)
