# This script supports shared and static libglfw3 simultaneously.

if(NOT DEFINED GLFW3_STATIC_INIT)
    set(GLFW3_STATIC_INIT OFF)
endif()
option(GLFW3_STATIC "By default, use static glfw3." ${GLFW3_STATIC_INIT})

if(MSVC)
    set(_lib_prefix )
    set(_lib_static_suffix .lib)
    set(_lib_shared_suffix .dll.lib)
else()
    set(_lib_prefix lib)
    if(WIN32)
        set(_lib_static_suffix .a)
        set(_lib_shared_suffix .dll.a)
    else()
        set(_lib_static_suffix .a)
        if(APPLE)
            set(_lib_shared_suffix .dylib)
        else()
            set(_lib_shared_suffix .so)
        endif()
    endif()
endif()

find_path(GLFW3_INCLUDE_DIR glfw3.h PATH_SUFFIXES GLFW)
find_library(GLFW3_STATIC_LIBRARY NAMES ${_lib_prefix}glfw3${_lib_static_suffix} ${_lib_prefix}glfw${_lib_static_suffix})
find_library(GLFW3_SHARED_LIBRARY NAMES ${_lib_prefix}glfw3${_lib_shared_suffix} ${_lib_prefix}glfw${_lib_shared_suffix})

set(_glfw3_required_variables )
if(GLFW3_STATIC)
    list(APPEND _glfw3_required_variables GLFW3_STATIC_LIBRARY)
else()
    list(APPEND _glfw3_required_variables GLFW3_SHARED_LIBRARY)
endif()
list(APPEND _glfw3_required_variables GLFW3_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(glfw3
    REQUIRED_VARS ${_glfw3_required_variables}
)

if(GLFW3_INCLUDE_DIR AND GLFW3_STATIC_LIBRARY AND NOT TARGET glfw_static)
    add_library(glfw_static STATIC IMPORTED)
    set_target_properties(glfw_static PROPERTIES
        IMPORTED_LOCATION "${GLFW3_STATIC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLFW3_INCLUDE_DIR}"
    )
endif()

if(GLFW3_INCLUDE_DIR AND GLFW3_SHARED_LIBRARY AND NOT TARGET glfw_shared)
    add_library(glfw_shared SHARED IMPORTED)
    set_target_properties(glfw_shared PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GLFW3_INCLUDE_DIR}"
    )
    if(WIN32)
        set_target_properties(glfw_shared PROPERTIES
            IMPORTED_IMPLIB "${GLFW3_SHARED_LIBRARY}"
        )
    else()
        set_target_properties(glfw_shared PROPERTIES
            IMPORTED_LOCATION "${GLFW3_SHARED_LIBRARY}"
        )
    endif()
endif()

if(glfw3_FOUND AND NOT TARGET glfw)
    add_library(glfw INTERFACE IMPORTED)
    if(GLFW3_STATIC)
        set_target_properties(glfw PROPERTIES
            INTERFACE_LINK_LIBRARIES "glfw_static"
        )
    else()
        set_target_properties(glfw PROPERTIES
            INTERFACE_LINK_LIBRARIES "glfw_shared"
        )
    endif()
endif()
