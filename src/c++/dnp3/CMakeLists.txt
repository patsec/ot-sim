include_directories(
  ${CPPZMQ_INCLUDE_DIRS}
  ${FMT_INCLUDE_DIRS}
  ${OPENDNP3_INCLUDE_DIRS}
  ${OTSIM_INCLUDE_DIRS}
)

file(GLOB_RECURSE ot-sim-dnp3_SRC *.cpp *.hpp)

add_library(ot-sim-dnp3 SHARED
  ${ot-sim-dnp3_SRC}
)

target_link_libraries(ot-sim-dnp3
  cppzmq
  fmt::fmt
  opendnp3
  ot-sim-msgbus
)

install(TARGETS ot-sim-dnp3
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
