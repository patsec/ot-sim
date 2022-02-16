include_directories(
  ${OPENDNP3_INCLUDE_DIRS}
)

add_executable(ot-sim-e2e-dnp3-master
  handler.hpp
  main.cpp
)

target_link_libraries(ot-sim-e2e-dnp3-master
  opendnp3
)

install(TARGETS ot-sim-e2e-dnp3-master
  RUNTIME DESTINATION bin
)