# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("coproto")

set(COMMON_FLAGS "-Wall -Wfatal-errors  -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")

add_subdirectory ("coproto")
add_subdirectory ("coprotoTests")
