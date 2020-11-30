find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 IMPORTED_TARGET "sdl2")
    if(TARGET PkgConfig::SDL2 AND NOT TARGET sdl2::sdl2)
        add_library(_sdl2 INTERFACE)
        target_link_libraries(_sdl2 INTERFACE PkgConfig::SDL2)
        add_library(SDL2::SDL2 ALIAS _sdl2)
    endif()
endif()

if(NOT SDL2_FOUND)
    find_path(SDL2_INCLUDE_DIR sdl2.h)
    find_library(SDL2_LIBRARY sdl2)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libuv
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
