# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(project4)

find_path(
  MAPREDUCE_INCLUDE_DIR
  NAMES "mapreduce.h" "mr_task_factory.h"
  PATHS ${CMAKE_CURRENT_SOURCE_DIR}
)

