

file(GLOB_RECURSE SRCS *.cpp)

include_directories(${CMAKE_SOURCE_DIR})


add_executable(coprotoTests  ${SRCS})

target_link_libraries(coprotoTests coproto)