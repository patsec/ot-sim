include_directories(
  ${CPPZMQ_INCLUDE_DIRS}
  ${FMT_INCLUDE_DIRS}
  ${SNAP7_INCLUDE_DIRS}
  ${OTSIM_INCLUDE_DIRS}
)

file(GLOB_RECURSE ot-sim-s7_SRC *.cpp *.hpp)

add_library(ot-sim-s7 SHARED
  ${ot-sim-s7_SRC}
)

target_link_libraries(ot-sim-s7
  cppzmq
  fmt::fmt
  snap7
  ot-sim-msgbus
)

install(TARGETS ot-sim-s7
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
