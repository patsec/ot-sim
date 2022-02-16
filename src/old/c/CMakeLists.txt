cmake_minimum_required(VERSION 3.11)

project(ot-sim LANGUAGES C)

set(JSON_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/deps
)

add_subdirectory(deps/json-c)

add_subdirectory(cmd/ot-sim-io-module)