add_executable(ska2anm
    ska2anm.cpp
)

target_link_libraries(ska2anm
    PUBLIC
        librw::librw
)

if(LIBRW_GL3_GFXLIB MATCHES "SDL[23]")
    target_compile_definitions(ska2anm PRIVATE SDL_MAIN_HANDLED)
endif()

if(LIBRW_INSTALL)
    install(TARGETS ska2anm
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

librw_platform_target(ska2anm INSTALL)
