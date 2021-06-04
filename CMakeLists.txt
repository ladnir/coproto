
cmake_minimum_required (VERSION 3.15)

project (coproto VERSION 0.2.0)


# make cache variables for install destinations
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Set a default build type for single-configuration
# CMake generators if no build type is set.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
endif()

option(COPROTO_CPP20    "compile the library with c++20 support" OFF)
option(COPROTO_LOGGING  "compile the library logging support" OFF)


message(STATUS "Option: CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Option: CMAKE_BUILD_TYPE     = ${CMAKE_BUILD_TYPE}\n\tRelease\n\tDebug\n\tRELWITHDEBINFO")


add_subdirectory ("coproto")
add_subdirectory ("coprotoTests")