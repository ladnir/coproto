
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/coprotoFindDeps.cmake")


file(GLOB_RECURSE SRCS *.cpp)

add_library(coproto STATIC ${SRCS})
target_include_directories(coproto PUBLIC 
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
                    $<INSTALL_INTERFACE:include>) 
target_include_directories(coproto PUBLIC 
                    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
                    $<INSTALL_INTERFACE:include>) 

if(NOT MSVC)
    target_compile_options(coproto PRIVATE "-Wfatal-errors")#
endif()

target_link_libraries(coproto Threads::Threads)
set_target_properties(coproto 
    PROPERTIES
    COPROTO_CPP20 ${COPROTO_CPP20}
    COPROTO_LOGGING ${COPROTO_LOGGING}
    )

message(STATUS "Option: COPROTO_CPP20    = ${COPROTO_CPP20}")
message(STATUS "Option: COPROTO_LOGGING  = ${COPROTO_LOGGING}\n")


if(COPROTO_CPP20)
    if(MSVC)
        target_compile_options(coproto PUBLIC
            "/await:strict" 
            "/std:c++20"
        )
    
    else()
        target_compile_options(coproto PUBLIC
            "-std=c++20" "-fcoroutines"
        )
    endif()

else()

    set_target_properties(coproto
        PROPERTIES
            CXX_STANDARD 11
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
    )
endif()

configure_file(config.h.in "config.h" )
configure_file("${CMAKE_CURRENT_LIST_DIR}/../cmake/coprotoFindDeps.cmake" "coprotoFindDeps.cmake" )


#############################################
#            Install                        #
#############################################

# generate the config file that is includes the exports
configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/../cmake/Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/coproto
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

if(NOT DEFINED coproto_VERSION_MAJOR)
    message("\n\n\n\n warning, coproto_VERSION_MAJOR not defined ${coproto_VERSION_MAJOR}")
endif()

set_property(TARGET coproto PROPERTY VERSION ${coproto_VERSION})

# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfigVersion.cmake"
  VERSION "${coproto_VERSION_MAJOR}.${coproto_VERSION_MINOR}.${coproto_VERSION_PATCH}"
  COMPATIBILITY AnyNewerVersion
)

# install the configuration file
install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfig.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/coprotoConfigVersion.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/coprotoFindDeps.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/coproto
)

# install library
install(
    TARGETS coproto 
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    EXPORT coprotoTargets)

# install headers
install(DIRECTORY . DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/coproto FILES_MATCHING PATTERN "*.h")

# install config
install(EXPORT coprotoTargets
  FILE coprotoTargets.cmake
  NAMESPACE coproto::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/coproto
)
 export(EXPORT coprotoTargets
       FILE "${CMAKE_CURRENT_BINARY_DIR}/coprotoTargets.cmake"
       NAMESPACE coproto::
)