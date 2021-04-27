cmake_minimum_required(VERSION 3.4)

project(outstation LANGUAGES C)

add_subdirectory(deps/json-c)

find_package(HELICS 2 REQUIRED)

add_executable(outstation outstation.c)
target_link_libraries(outstation HELICS::helicsSharedLib modbus pthread)
# set_target_properties(outstation PROPERTIES FOLDER c/federates)
