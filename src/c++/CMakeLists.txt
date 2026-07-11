set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")

set(CPPZMQ_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/deps
)

set(FMT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt/include
)

set(JSON_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/json/single_include
)

set(OPENDNP3_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/opendnp3/cpp/lib/include
)

set(SNAP7_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/deps/snap7
)

set(OTSIM_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CPPZMQ_BUILD_TESTS OFF)
set(JSON_BuildTests OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(deps/cppzmq)
add_subdirectory(deps/fmt)
add_subdirectory(deps/json)
add_subdirectory(deps/opendnp3)

add_subdirectory(dnp3)
add_subdirectory(s7)
add_subdirectory(msgbus)

add_subdirectory(cmd/ot-sim-dnp3-module)
add_subdirectory(cmd/ot-sim-s7comm-module)

if(BUILD_E2E)
  add_subdirectory(cmd/ot-sim-e2e-dnp3-master)
endif()
