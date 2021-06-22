add_executable(imguitest WIN32
    main.cpp
)

target_link_libraries(imguitest
    PUBLIC
        librw::skeleton
        librw::librw
)

if(LIBRW_INSTALL)
    install(TARGETS imguitest
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
endif()

librw_platform_target(imguitest INSTALL)
