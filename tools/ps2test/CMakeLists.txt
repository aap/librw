add_executable(ps2test
    gs.h
    main.cpp
    mem.h
    ps2.h
    vu/defaultpipe.dsm
    vu/skinpipe.dsm
)

target_link_libraries(ps2test
    PRIVATE
        librw::librw
        kernel
)

librw_platform_target(ps2test)

if(LIBRW_INSTALL)
    install(TARGETS ps2test
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()
