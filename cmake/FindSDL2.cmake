# This script supports shared and static SDL2 simultaneously.

if(NOT DEFINED SDL2_STATIC_INIT)
    set(SDL2_STATIC_INIT OFF)
endif()
option(SDL2_STATIC "By default, use static SDL2." ${SDL2_STATIC_INIT})

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

find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
find_library(SDL2_STATIC_LIBRARY NAMES ${_lib_prefix}SDL2${_lib_static_suffix})
find_library(SDL2_SHARED_LIBRARY NAMES ${_lib_prefix}SDL2${_lib_shared_suffix})

# SDL2main is always a static library
find_library(SDL2_SDL2main_LIBRARY ${_lib_prefix}SDL2main${_lib_static_suffix})

set(_sdl2_required_variables )
if(SDL2_STATIC)
    list(APPEND _sdl2_required_variables SDL2_STATIC_LIBRARY)
else()
    list(APPEND _sdl2_required_variables SDL2_SHARED_LIBRARY)
endif()
list(APPEND _sdl2_required_variables SDL2_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
    REQUIRED_VARS ${_sdl2_required_variables}
)

set(SDL2_SHARED_EXTRA_LIBRARIES CACHE STRING "SDL2 extra shared libraries")
if(WIN32)
    set(SDL2_STATIC_EXTRA_LIBRARIES imm32 setupapi version winmm CACHE STRING "SDL2 extra static libraries")
else()
    set(SDL2_STATIC_EXTRA_LIBRARIES  CACHE STRING "SDL2 extra static libraries")
endif()

if(SDL2_INCLUDE_DIR AND SDL2_STATIC_LIBRARY AND NOT TARGET SDL2::SDL2_static)
    add_library(SDL2::SDL2_static STATIC IMPORTED)
    set_target_properties(SDL2::SDL2_static PROPERTIES
        IMPORTED_LOCATION "${SDL2_STATIC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${SDL2_STATIC_EXTRA_LIBRARIES}"
    )
endif()

if(SDL2_INCLUDE_DIR AND SDL2_SHARED_LIBRARY AND NOT TARGET SDL2::SDL2_shared)
    add_library(SDL2::SDL2_shared SHARED IMPORTED)
    set_target_properties(SDL2::SDL2_shared PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${SDL2_SHARED_EXTRA_LIBRARIES}"
    )
    if(WIN32)
        set_target_properties(SDL2::SDL2_shared PROPERTIES
            IMPORTED_IMPLIB "${SDL2_SHARED_LIBRARY}"
        )
    else()
        set_target_properties(SDL2::SDL2_shared PROPERTIES
            IMPORTED_LOCATION "${SDL2_SHARED_LIBRARY}"
        )
    endif()
endif()

if(SDL2_FOUND AND NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    if(SDL2_STATIC)
        set_target_properties(SDL2::SDL2 PROPERTIES
            INTERFACE_LINK_LIBRARIES "SDL2::SDL2_static"
        )
    else()
        set_target_properties(SDL2::SDL2 PROPERTIES
            INTERFACE_LINK_LIBRARIES "SDL2::SDL2_shared"
        )
    endif()
endif()

if(SDL2_SDL2main_LIBRARY AND NOT TARGET SDL2::SDL2main)
    message(STATUS "SDL2main is available")
    add_library(SDL2::SDL2main STATIC IMPORTED)
    set_target_properties(SDL2::SDL2main PROPERTIES
        IMPORTED_LOCATION "${SDL2_SDL2main_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
else()
    message(STATUS "SDL2main is NOT available")
endif()

