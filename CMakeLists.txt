cmake_minimum_required(VERSION 3.2)
set(PROJ_NAME RPPP)
project(${PROJ_NAME} CXX)
set(CMAKE_CXX_STANDARD 17)

set(PROJECT_SOURCE_DIR src)
set(PROJECT_INCLUDE_DIR include)
aux_source_directory(${PROJECT_SOURCE_DIR} SRC_FILES)

add_subdirectory(test)