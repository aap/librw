cmake_minimum_required(VERSION 3.8)
project(librw C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

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

set(LIBRW_GL3_GFXLIBS "GLFW" "SDL2")
set(LIBRW_GL3_GFXLIB "GLFW" CACHE STRING "gfxlib for gl3")
set_property(CACHE LIBRW_GL3_GFXLIB PROPERTY STRINGS ${LIBRW_GL3_GFXLIBS})
if(LIBRW_PLATFORM_GL3)
    message(STATUS "LIBRW_GL3_GFXLIB = ${LIBRW_GL3_GFXLIB} (choices=${LIBRW_GL3_GFXLIBS})")
endif()
if(NOT LIBRW_GL3_GFXLIB IN_LIST LIBRW_GL3_GFXLIBS)
    message(FATAL_ERROR "Illegal LIBRW_GL3_GFXLIB=${LIBRW_GL3_GFXLIB}")
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
    configure_package_config_file(cmake/librw-config.cmake.in librw-config.cmake
        INSTALL_DESTINATION "${CMAKE_INSTALL_PREFIX}"
        )
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/librw-config.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        )
    install(
        EXPORT librw-targets NAMESPACE librw::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
        )
    set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A (partial) re-implementation of RenderWare Graphics")
    set(CPACK_PACKAGE_VENDOR "aap")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
    set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}")
    include(CPack)
endif()
