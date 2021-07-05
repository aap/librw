# On Windows, SDL2main is included as part of the pkg-config output, which isn't desirable when linking.
if(NOT WIN32)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET "sdl2")
        if(TARGET PkgConfig::SDL2 AND NOT TARGET SDL2::SDL2)
            add_library(SDL2::SDL2 INTERFACE IMPORTED)
            set_property(TARGET SDL2::SDL2 PROPERTY INTERFACE_LINK_LIBRARIES PkgConfig::SDL2)
        endif()
    endif()
endif()

if(NOT SDL2_FOUND)
    find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
    find_library(SDL2_LIBRARY SDL2 SDL2d)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(SDL2
        REQUIRED_VARS SDL2_INCLUDE_DIR SDL2_LIBRARY
    )

    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
        )
    endif()
endif()

find_library(SDL2main_LIBRARY SDL2main)

if(SDL2main_LIBRARY AND NOT TARGET SDL2::SDL2main)
    add_library(SDL2::SDL2main UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2main PROPERTIES
        IMPORTED_LOCATION "${SDL2main_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
    )
endif()
