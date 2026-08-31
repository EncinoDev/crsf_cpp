cmake_minimum_required(VERSION 3.20)
project(CrsfCpp VERSION 0.1.0 LANGUAGES CXX)

add_library(crsf_cpp INTERFACE)
add_library(crsf::crsf ALIAS crsf_cpp)

target_compile_features(crsf_cpp INTERFACE cxx_std_20)
target_include_directories(
    crsf_cpp INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                       $<INSTALL_INTERFACE:include>)

include(GNUInstallDirs)
install(TARGETS crsf_cpp EXPORT crsfTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    EXPORT crsfTargets
    NAMESPACE crsf::
    FILE crsfConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/crsf)

set(_crsf_standalone OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_crsf_standalone ON)
endif()

if(NOT DEFINED CRSF_BUILD_TESTS)
    option(CRSF_BUILD_TESTS "Build crsf_cpp tests" ON)
endif()

set(_crsf_embedded OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    set(_crsf_embedded ON)
endif()

if(NOT _crsf_embedded AND CRSF_BUILD_TESTS)
    if(_crsf_standalone)
        enable_testing()
    endif()
    add_subdirectory(tests)
endif()
