add_executable(ska2anm
    ska2anm.cpp
)

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
