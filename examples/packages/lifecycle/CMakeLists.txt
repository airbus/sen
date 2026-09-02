add_sen_package(
  TARGET lifecycle
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "the object lifecycle hooks, shown in howto_guides/objects.md"
  SOURCES src/phase_recorder.h src/phase_recorder.cpp
  STL_FILES stl/lifecycle/phase_recorder.stl
)
