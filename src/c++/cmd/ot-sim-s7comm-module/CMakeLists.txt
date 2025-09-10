find_package(Boost REQUIRED)

include_directories(
  ${Boost_INCLUDE_DIRS}
  ${CPPZMQ_INCLUDE_DIRS}
  ${FMT_INCLUDE_DIRS}
  ${OTSIM_INCLUDE_DIRS}
  ${SNAP7_INCLUDE_DIRS}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

add_executable(ot-sim-s7comm-module
  main.cpp
  ${SNAP7_INCLUDE_DIRS}/snap7.cpp
)

target_link_libraries(ot-sim-s7comm-module
  ${Boost_LIBRARIES}
  fmt::fmt
  ot-sim-msgbus
  ot-sim-s7
)

install(TARGETS ot-sim-s7comm-module
  RUNTIME DESTINATION bin
)