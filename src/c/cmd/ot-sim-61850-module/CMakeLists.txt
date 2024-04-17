find_package(LibXml2 REQUIRED)

include_directories(
  ${JSON_INCLUDE_DIRS}
  ${LIBXML2_INCLUDE_DIR}
  include
)

file(GLOB ot-sim-61850-module_SRC
  "src/*.c"
)

add_executable(ot-sim-61850-module
  ${ot-sim-61850-module_SRC}
  main.c
)

target_link_libraries(ot-sim-61850-module
  czmq
  iec61850
  ${LIBXML2_LIBRARIES}
  json-c
  pthread
  zmq
)

install(TARGETS ot-sim-61850-module
  RUNTIME DESTINATION bin
)
