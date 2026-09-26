add_sen_package(
  TARGET crash_demo
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "an object that fails on purpose, shown in users_guide/crash_reports.md"
  SOURCES src/faulty.h src/faulty.cpp
  STL_FILES stl/crash_demo/faulty.stl
)
