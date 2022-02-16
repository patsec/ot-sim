include_directories(
  ${CPPZMQ_INCLUDE_DIRS}
  ${JSON_INCLUDE_DIRS}
)

file(GLOB_RECURSE ot-sim-msgbus_SRC *.cpp *.hpp)

add_library(ot-sim-msgbus SHARED
  ${ot-sim-msgbus_SRC}
)

target_link_libraries(ot-sim-msgbus
  cppzmq
  nlohmann_json
)

install(TARGETS ot-sim-msgbus
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)