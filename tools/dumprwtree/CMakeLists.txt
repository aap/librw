add_executable(dumprwtree
    dumprwtree.cpp
)

target_link_libraries(dumprwtree
    PUBLIC
        librw
)

if(LIBRW_INSTALL)
    install(TARGETS dumprwtree
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

if(NINTENDO_SWITCH)
    nx_generate_nacp(dumprwtree.nacp
        NAME "dumprwtree"
        AUTHOR "${librw_AUTHOR}"
        VERSION "${librw_VERSION}"
    )

    nx_create_nro(dumprwtree
        NACP dumprwtree.nacp
    )

    if(LIBRW_INSTALL)
        install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dumprwtree.nro"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
    endif()
endif()
