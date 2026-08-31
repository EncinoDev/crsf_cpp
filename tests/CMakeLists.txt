include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(crsf_cpp_tests smoke_test.cpp)
target_link_libraries(crsf_cpp_tests PRIVATE crsf::crsf GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(crsf_cpp_tests)
