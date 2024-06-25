# exe4cpp
Executor abstractions for C++ with implementations for ASIO 

## Usage
This project is a header-only library. You can safely copy the `src` folder and add it in the include directories
to be up and running.

The project also provides a CMake configuration. You will need a recent version of CMake (3.8 and higher).

* Copy/clone this repository in your project (or use [git submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules))
* `add_subdirectory` the repository
* For every target that needs this library, `target_link_libraries(foo PUBLIC|PRIVATE|INTERFACE ser4cpp)`

## Tests
To build and run the tests of this project, follow these steps:

* Clone this repository
* `mkdir build`
* `cd build`
* `cmake ..`
* `cmake --build .`
* `ctest .`

If you already have a CMake cache, be sure to set the `EXE4CPP_BUILD_TESTS` cache variable to `ON` in order to build the tests.

## License
This project is licensed under the terms of the BSD v3 license. See `LICENSE` for more details.