add_executable(ska2anm
    ska2anm.cpp
)

if(LIBRW_PLATFORM_GL3 AND LIBRW_GL3_GFXLIB STREQUAL "SDL2")
    target_compile_definitions(ska2anm
        PRIVATE
            SDL_MAIN_HANDLED
    )
endif()

target_link_libraries(ska2anm
    PUBLIC
        librw::librw
)

librw_platform_target(ska2anm INSTALL)

if(LIBRW_INSTALL)
    install(TARGETS ska2anm
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()
