add_executable(demoreel WIN32
    demoreel.h
    main.cpp
    proctex.cpp
    scene_knot.cpp
    scene_particles.cpp
    scene_tunnel.cpp
)

target_link_libraries(demoreel
    PRIVATE
        librw::skeleton
        librw::librw
)

librw_platform_target(demoreel)
