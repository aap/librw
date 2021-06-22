add_executable(subrast WIN32
    main.cpp subrast.cpp
)

target_link_libraries(subrast
    PUBLIC
        librw::skeleton
        librw::librw
)

librw_platform_target(subrast)
