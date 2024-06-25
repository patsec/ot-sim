# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(ser4cpp-populate NONE)



include(ExternalProject)
ExternalProject_Add(ser4cpp-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://github.com/automatak/ser4cpp/archive/3c449734dc530a8f465eb0982de29165cc4e23d5.zip" "URL_HASH" "SHA1=937B759B7CC80180DA26B47037E796B59798A672"
                    SOURCE_DIR          "/home/htoohey/repos/ot-sim/_deps/ser4cpp-src"
                    BINARY_DIR          "/home/htoohey/repos/ot-sim/_deps/ser4cpp-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


