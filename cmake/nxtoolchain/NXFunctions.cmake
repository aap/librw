if(NOT COMMAND nx_generate_nacp)
    message(FATAL_ERROR "The `nx_generate_nacp` cmake command is not available. Please use an appropriate Nintendo Switch toolchain.")
endif()

if(NOT COMMAND nx_create_nro)
    message(FATAL_ERROR "The `nx_create_nro` cmake command is not available. Please use an appropriate Nintendo Switch toolchain.")
endif()

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

function(librw_platform_target TARGET)
    cmake_parse_arguments(LPT "INSTALL" "" "" ${ARGN})

    get_target_property(TARGET_TYPE "${TARGET}" TYPE)
    if(TARGET_TYPE STREQUAL "EXECUTABLE")
        nx_generate_nacp(${TARGET}.nacp
            NAME "${TARGET}"
            AUTHOR "${librw_AUTHOR}"
            VERSION "${librw_VERSION}"
        )

        nx_create_nro(${TARGET}
            NACP ${TARGET}.nacp
        )

        if(LIBRW_INSTALL AND LPT_INSTALL)
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.nro"
                DESTINATION "${CMAKE_INSTALL_BINDIR}"
            )
        endif()
    endif()
endfunction()
