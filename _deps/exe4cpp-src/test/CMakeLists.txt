set(catch_header ./catch/catch.hpp)

set(exe4cpp_tests_src
    ./main.cpp
    ./TestMockExecutor.cpp  
)

set(exe4cpp_asio_tests_src
    ./asio/TestBasicExecutor.cpp
    ./asio/TestStrandExecutor.cpp
)

add_executable(exe4cpp_tests ${catch_header} ${exe4cpp_tests_src})
target_compile_features(exe4cpp_tests PRIVATE cxx_std_14)
target_link_libraries(exe4cpp_tests PRIVATE exe4cpp)
target_include_directories(exe4cpp_tests PRIVATE ./catch)

if(TARGET asio)
    target_sources(exe4cpp_tests PRIVATE ${exe4cpp_asio_tests_src})
endif()

add_test(exe4cpp_tests exe4cpp_tests)
