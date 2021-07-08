add_executable(dumprwtree
    dumprwtree.cpp
)

if(LIBRW_PLATFORM_GL3 AND LIBRW_GL3_GFXLIB STREQUAL "SDL2")
    target_compile_definitions(dumprwtree
        PRIVATE
            SDL_MAIN_HANDLED
    )
endif()

target_link_libraries(dumprwtree
    PRIVATE
        librw::librw
)

if(LIBRW_INSTALL)
    install(TARGETS dumprwtree
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

librw_platform_target(dumprwtree INSTALL)
