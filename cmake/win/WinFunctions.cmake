set(SDL2_STATIC_INIT ON)
set(GLFW3_STATIC_INIT ON)

set(LIBRW_STATIC_RUNTIME_DEFAULT ON)

function(librw_platform_target TARGET)
    cmake_parse_arguments(LPT "PROVIDES_WINMAIN" "" "" ${ARGN})
    get_target_property(TARGET_TYPE "${TARGET}" TYPE)

    if(MINGW)
        if(LPT_PROVIDES_WINMAIN AND TARGET_TYPE MATCHES "^(INTERFACE|SHARED|STATIC)_LIBRARY$")
            # Start with the WinMain symbol marked as undefined.
            # This will prevent the linker ignoring a WinMain symbol.
            if(CMAKE_SIZEOF_VOID_P EQUAL 4)
                target_link_options(${TARGET} INTERFACE -Wl,--undefined,_WinMain@16)
            endif()
            target_link_options(${TARGET} INTERFACE -Wl,--undefined,WinMain)
        endif()
    endif()

    set_target_properties(${TARGET} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET}>"
    )
endfunction()
