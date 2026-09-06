cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(my_package_project VERSION 0.0.1 LANGUAGES CXX C)

if(DEFINED ENV{SEN_PREFIX})
   list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PREFIX}/cmake")
endif()

find_package(sen REQUIRED)

add_sen_package(
  TARGET
    my_package
  MAINTAINER
    "<your name goes here>"
  VERSION
    "0.0.1"
  DESCRIPTION
    "<package description goes here>"
  SOURCES
    src/my_class.h
    src/my_class.cpp
  STL_FILES
    stl/my_package/my_class.stl
    stl/my_package/basic_types.stl
)
