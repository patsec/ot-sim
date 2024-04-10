find_package(LibXml2 REQUIRED)

include_directories(
  ${JSON_INCLUDE_DIRS}
  ${LIBXML2_INCLUDE_DIR}
)

add_executable(ot-sim-61850-module
  main.c
)

target_link_libraries(ot-sim-61850-module
  czmq
  ${LIBXML2_LIBRARIES}
  json-c
  pthread
  zmq
)

install(TARGETS ot-sim-61850-module
  RUNTIME DESTINATION bin
)
