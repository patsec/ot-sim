set(catch_header ./catch/catch.hpp)

set(ser4cpp_tests_src
    ./main.cpp
    ./TestBigEndianSerialization.cpp
	./TestBuffer.cpp
    ./TestFloatSerialization.cpp
	./TestHexConversions.cpp
    ./TestLittleEndianSerialization.cpp
    ./TestStaticBuffer.cpp    
)

add_executable(ser4cpp_tests ${catch_header} ${ser4cpp_tests_src})
target_compile_features(ser4cpp_tests PRIVATE cxx_std_14)
target_link_libraries(ser4cpp_tests PRIVATE ser4cpp)
target_include_directories(ser4cpp_tests PRIVATE ./catch)

add_test(ser4cpp_tests ser4cpp_tests)
