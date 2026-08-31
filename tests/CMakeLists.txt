include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
    crsf_cpp_tests
    smoke_test.cpp
    crc8_test.cpp
    bit_packing_test.cpp
    parser_test.cpp
    round_trip_test.cpp
    vectors_test.cpp)
target_link_libraries(crsf_cpp_tests PRIVATE crsf::crsf GTest::gtest_main)
target_compile_definitions(crsf_cpp_tests PRIVATE CRSF_TEST_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/vectors")

include(GoogleTest)
# Cross builds only prove the library compiles for the target; the host cannot run the binary to
# enumerate its tests.
if(NOT CMAKE_CROSSCOMPILING)
    gtest_discover_tests(crsf_cpp_tests)
endif()

add_executable(crsf_cpp_bench bench/crsf_bench.cpp)
target_link_libraries(crsf_cpp_bench PRIVATE crsf::crsf)

option(CRSF_BUILD_FUZZERS "Build libFuzzer targets (requires Clang)" OFF)
if(CRSF_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "CRSF_BUILD_FUZZERS requires Clang, got ${CMAKE_CXX_COMPILER_ID}")
    endif()
    add_executable(crsf_cpp_parser_fuzz fuzz/parser_fuzz.cpp)
    target_link_libraries(crsf_cpp_parser_fuzz PRIVATE crsf::crsf)
    target_compile_options(crsf_cpp_parser_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(crsf_cpp_parser_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
