find_package(Boost REQUIRED)

include_directories(
  ${Boost_INCLUDE_DIRS}
  ${CPPZMQ_INCLUDE_DIRS}
  ${FMT_INCLUDE_DIRS}
  ${OPENDNP3_INCLUDE_DIRS}
  ${OTSIM_INCLUDE_DIRS}
)

link_directories(
  ${Boost_LIBRARY_DIRS}
)

add_definitions(-DBOOST_ALL_NO_LIB -DBOOST_ALL_DYN_LINK)

add_executable(ot-sim-dnp3-module
  main.cpp
)

target_link_libraries(ot-sim-dnp3-module
  ${Boost_LIBRARIES}
  fmt::fmt
  ot-sim-dnp3
  ot-sim-msgbus
)

install(TARGETS ot-sim-dnp3-module
  RUNTIME DESTINATION bin
)