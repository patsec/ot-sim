# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.22.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(exe4cpp-populate NONE)



include(ExternalProject)
ExternalProject_Add(exe4cpp-populate
                     "UPDATE_DISCONNECTED" "False" "URL" "https://github.com/automatak/exe4cpp/archive/fb878a4de598ba9d6e4338afebf83f96e03af1b8.zip" "URL_HASH" "SHA1=18B141E8CF09DC8D28CC62DD5FA2920670D501BD"
                    SOURCE_DIR          "/home/htoohey/repos/ot-sim/_deps/exe4cpp-src"
                    BINARY_DIR          "/home/htoohey/repos/ot-sim/_deps/exe4cpp-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)


