cmake_minimum_required(VERSION 3.8)

project(librw C CXX)

if(PS2DEV AND EE)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ee.cmake")
endif()

if(WIN32)
    set(LIBRW_PLATFORMS "NULL" "GL3" "D3D9")
else()
    set(LIBRW_PLATFORMS "NULL" "GL3" "PS2")
endif()
set(LIBRW_PLATFORM "NULL" CACHE STRING "Platform")
set_property(CACHE LIBRW_PLATFORM PROPERTY STRINGS ${LIBRW_PLATFORMS})
message(STATUS "LIBRW_PLATFORM = ${LIBRW_PLATFORM} (choices=${LIBRW_PLATFORMS})")
set("LIBRW_PLATFORM_${LIBRW_PLATFORM}" ON)
if(NOT LIBRW_PLATFORM IN_LIST LIBRW_PLATFORMS)
    message(FATAL_ERROR "Illegal LIBRW_PLATFORM=${LIBRW_PLATFORM}")
endif()

if(LIBRW_PLATFORM_PS2)
    enable_language(DSM)
endif()

option(LIBRW_TOOLS "Build librw tools" ON)
option(LIBRW_INSTALL "Install librw files" OFF)

if(LIBRW_INSTALL)
    include(GNUInstallDirs)
    set(LIBRW_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/librw")
endif()

add_subdirectory(src)

if(LIBRW_TOOLS)
    add_subdirectory(skeleton)

    add_subdirectory(tools)
endif()

if(LIBRW_INSTALL)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(librw-config.cmake.in librw-config.cmake
        INSTALL_DESTINATION "${CMAKE_INSTALL_PREFIX}"
        )
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/librw-config.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        )
    install(
        EXPORT librw-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        )

    include(CMakeCPack.cmake)
endif()
