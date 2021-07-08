set(SDL2_STATIC_INIT ON)
set(GLFW3_STATIC_INIT ON)

set(LIBRW_STATIC_RUNTIME_DEFAULT ON)

option(LIBRW_INSTALL_RUNTIME_DEPS "Install runtime dependencies of executables" OFF)

function(librw_platform_target TARGET)
    cmake_parse_arguments(LPT "INSTALL;PROVIDES_WINMAIN" "" "" ${ARGN})
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

    if(LIBRW_INSTALL_RUNTIME_DEPS)
        # Copy runtime dependencies (glfw3/SDL2 library + gcc runtime libraries)
        if(LPT_INSTALL AND TARGET_TYPE MATCHES "^EXECUTABLE$")
            cmake_minimum_required(VERSION 3.14)
            # FIXME: Once CMake 3.21 becomes common, replace with `install(RUNTIME_DEPENDENCY_SET)`
            set(install_code [=[
                set(runtime_extra_directories)
                execute_process(COMMAND uname OUTPUT_VARIABLE uname OUTPUT_VARIABLE uname_out RESULT_VARIABLE uname_res)
                if(uname_res EQUAL 0)
                    string(STRIP "${uname_out}" uname_out)
                    if (uname_out MATCHES "^(MINGW|mingw)(32|64)" AND DEFINED ENV{MSYSTEM_PREFIX})
                        execute_process(COMMAND cygpath -m "$ENV{MSYSTEM_PREFIX}/bin" OUTPUT_VARIABLE cygpath_out RESULT_VARIABLE cygpath_res)
                        if(cygpath_res EQUAL 0)
                            string(STRIP "${cygpath_out}" cygpath_out)
                            list(APPEND runtime_extra_directories "${cygpath_out}")
                        endif()
                    endif()
                endif()

                file(GET_RUNTIME_DEPENDENCIES
                    RESOLVED_DEPENDENCIES_VAR var
                    UNRESOLVED_DEPENDENCIES_VAR unvar
                    EXECUTABLES $<TARGET_FILE:__TARGET__>
                    DIRECTORIES ${runtime_extra_directories}
                )
                file(COPY ${var} DESTINATION $ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/__CMAKE_INSTALL_BINDIR__
                    FILES_MATCHING
                        REGEX ".*(lib)?(GLFW|glfw|SDL2|sdl2).*\\.dll$"
                        REGEX ".*lib(gcc|stdc\\+\\+|winpthread).*\\.dll$"
                )
            ]=])
            string(REPLACE __TARGET__ "${TARGET}" install_code ${install_code})
            string(REPLACE __CMAKE_INSTALL_BINDIR__ "${CMAKE_INSTALL_BINDIR}" install_code ${install_code})
            install(CODE "${install_code}")
        endif()
    endif()
endfunction()
