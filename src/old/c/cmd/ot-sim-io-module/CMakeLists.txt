find_package(HELICS REQUIRED)
find_package(LibXml2 REQUIRED)

include_directories(
  ${JSON_INCLUDE_DIRS}
  ${LIBXML2_INCLUDE_DIR}
)

add_executable(ot-sim-io-module
  main.c
)

target_link_libraries(ot-sim-io-module
  czmq
  ${LIBXML2_LIBRARIES}
  ${HELICS_C_SHARED_LIBRARY}
  json-c
  pthread
  zmq
)

install(TARGETS ot-sim-io-module
  RUNTIME DESTINATION bin
)