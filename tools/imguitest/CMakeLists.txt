add_executable(imguitest WIN32
    main.cpp
)

target_link_libraries(imguitest
    PUBLIC
        librw
        librw_skeleton_imgui
)

if(LIBRW_INSTALL)
    install(TARGETS imguitest
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

if(NINTENDO_SWITCH)
    nx_generate_nacp(imguitest.nacp
        NAME "librw imguitest"
        AUTHOR "${librw_AUTHOR}"
        VERSION "${librw_VERSION}"
    )

    nx_create_nro(imguitest
        NACP imguitest.nacp
    )

    if(LIBRW_INSTALL)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/imguitest.nro"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
    endif()
endif()
