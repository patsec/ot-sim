# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(asio-populate NONE)



include(ExternalProject)
ExternalProject_Add(asio-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://github.com/chriskohlhoff/asio/archive/asio-1-16-0.zip" "URL_HASH" "SHA1=6BDD33522D5B95B36445ABB2072A481F7CE15402"
                    SOURCE_DIR          "/home/htoohey/repos/ot-sim/_deps/asio-src"
                    BINARY_DIR          "/home/htoohey/repos/ot-sim/_deps/asio-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


