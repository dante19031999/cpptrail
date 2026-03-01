cmake_minimum_required(VERSION 3.31)
project(cpptrail VERSION 1.0.0 LANGUAGES CXX)

# Setup C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(CPPTRAIL_BUILD_TESTS "Build CppTrail unit tests" OFF)

# External components
include(${CMAKE_CURRENT_LIST_DIR}/cmake/doxygen.cmake)

# Library targets
add_library(cpptrail INTERFACE)
add_library(CppTrail::cpptrail ALIAS cpptrail)

# Include directories
# 'BUILD_INTERFACE' is for those who are developing inside this repo
# 'INSTALL_INTERFACE' is for when someone else installs the lib
target_include_directories(cpptrail INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Debug
#if (CPPTRAIL_BUILD_TESTS)
    # Test executable
    add_executable(cpptrail_test src/cpptrail/main.cpp)
    target_link_libraries(cpptrail_test PRIVATE CppTrail::cpptrail)
    # Add test
    enable_testing()
    add_test(NAME AllTests COMMAND cpptrail_test)
#endif ()

# Documentation target
add_custom_target(doc
        COMMAND ${DOXYGEN} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
)
