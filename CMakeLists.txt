cmake_minimum_required(VERSION 3.4)

project(outstation LANGUAGES C)

# file(GLOB outstation_SRC CONFIGURE_DEPENDS "*.c")

set(CMAKE_BUILD_TYPE Debug)
set(SRC_FILES ${PROJECT_SOURCE_DIR}/outstation.c
              ${PROJECT_SOURCE_DIR}/register.c
)

add_subdirectory(deps/json-c)

find_package(HELICS 2 REQUIRED)

add_executable(outstation ${SRC_FILES})
target_link_libraries(outstation HELICS::helicsSharedLib json-c modbus pthread)