add_executable(dumprwtree
    dumprwtree.cpp
)

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
