find_package(LibXml2 REQUIRED)

include_directories(
  ${LIBXML2_INCLUDE_DIR}
)

add_executable(ot-sim-message-bus
  main.c
)

target_link_libraries(ot-sim-message-bus
  czmq
  ${LIBXML2_LIBRARIES}
  zmq
)

install(TARGETS ot-sim-message-bus
  RUNTIME DESTINATION bin
)