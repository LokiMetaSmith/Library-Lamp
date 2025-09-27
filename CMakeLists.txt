# This is the top-level CMakeLists.txt file for the project.

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS sqlite3)
set(PARTITION_TABLE_CSV "partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

#idf_build_set_property(MINIMAL_BUILD ON)
get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" ProjectId ${ProjectId})
project(${ProjectId})