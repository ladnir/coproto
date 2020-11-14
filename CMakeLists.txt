
cmake_minimum_required (VERSION 3.8)

project ("coproto")

set(COMMON_FLAGS " -Wfatal-errors  ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")



# Set a default build type for single-configuration
# CMake generators if no build type is set.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Release)
endif()

option(COPROTO_CPP20    "compile the library with c++20 support" OFF)
option(COPROTO_LOGGING  "compile the library logging support" OFF)



message(STATUS "Option: CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}\n\tRelease\n\tDebug\n\tRELWITHDEBINFO")



add_subdirectory ("coproto")
add_subdirectory ("coprotoTests")


set_target_properties( coproto coprotoTests
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)