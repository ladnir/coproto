


file(GLOB_RECURSE SRCS *.cpp)

set(SRCS "${SRCS}")

add_library(coproto STATIC ${SRCS})
target_include_directories(coproto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..) 

find_package( Threads REQUIRED)
target_link_libraries(coproto Threads::Threads)


message(STATUS "Option: COPROTO_CPP20    = ${COPROTO_CPP20}")
message(STATUS "Option: COPROTO_LOGGING  = ${COPROTO_LOGGING}\n")


if(COPROTO_CPP20)

    target_compile_options(coproto PUBLIC
        "-std=c++2a" "-fcoroutines"
    )

else()

    set_target_properties(coproto
        PROPERTIES
            CXX_STANDARD 11
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
endif()


configure_file(configCMake.h.in "${CMAKE_CURRENT_SOURCE_DIR}/configCMake.h" )


#############################################
#            Install                        #
#############################################

# install library
install(TARGETS coproto DESTINATION lib)

# install headers
install(DIRECTORY . DESTINATION include/coproto FILES_MATCHING PATTERN "*.h")
