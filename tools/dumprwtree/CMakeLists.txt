add_executable(dumprwtree
    dumprwtree.cpp
)

target_link_libraries(dumprwtree
    PRIVATE
        librw::librw
)

if(LIBRW_GL3_GFXLIB MATCHES "SDL[23]")
    target_compile_definitions(dumprwtree PRIVATE SDL_MAIN_HANDLED)
endif()

if(LIBRW_INSTALL)
    install(TARGETS dumprwtree
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

librw_platform_target(dumprwtree INSTALL)
