# Install script for directory: /home/htoohey/repos/ot-sim/src/c++

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/htoohey/repos/ot-sim/src/c++/deps/cppzmq/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/deps/fmt/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/deps/json/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/deps/opendnp3/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/dnp3/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/snap7/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/msgbus/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/cmd/ot-sim-dnp3-module/cmake_install.cmake")
  include("/home/htoohey/repos/ot-sim/src/c++/cmd/ot-sim-s7comm-module/cmake_install.cmake")

endif()

