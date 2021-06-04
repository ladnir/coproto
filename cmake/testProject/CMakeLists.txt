
cmake_minimum_required (VERSION 3.15)

project (TestProj VERSION  1.0.0)

add_executable(main "src.cpp")

message("CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}")

find_package(COPROTO 0.2.0 EXACT REQUIRED)

target_link_libraries(main coproto::coproto)

message(STATUS "COPROTO_CPP20   = ${COPROTO_CPP20}")
message(STATUS "COPROTO_LOGGING = ${COPROTO_LOGGING}")