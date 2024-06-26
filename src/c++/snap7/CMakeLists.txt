include_directories(
  ${CPPZMQ_INCLUDE_DIRS}
  ${FMT_INCLUDE_DIRS}
  ${OTSIM_INCLUDE_DIRS}
)

file(GLOB_RECURSE ot-sim-snap7_SRC *.cpp *.hpp)

add_library(ot-sim-snap7 SHARED
  ${ot-sim-snap7}
)

target_link_libraries(ot-sim-snap7
  cppzmq
  fmt::fmt
  ot-sim-msgbus
)

install(TARGETS ot-sim-snap7
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
