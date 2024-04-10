set(JSON_INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/deps
)

add_subdirectory(deps/json-c)

add_subdirectory(cmd/ot-sim-61850-module)
add_subdirectory(cmd/ot-sim-message-bus)
