cmake_minimum_required(VERSION 3.11)

project(ot-sim)

find_package(Git QUIET)
if (GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  message(STATUS "Executing git submodule update")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE GIT_SUBMOD_RESULT
  )

  if (NOT GIT_SUBMOD_RESULT EQUAL "0")
    message(
      FATAL_ERROR "git submodule update failed with ${GIT_SUBMOD_RESULT}"
    )
  endif ()
endif ()

OPTION(BUILD_E2E "Build E2E test executables" OFF)

add_subdirectory(src/c)
add_subdirectory(src/c++)
