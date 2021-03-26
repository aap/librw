add_executable(subrast WIN32
    main.cpp subrast.cpp
)

target_link_libraries(subrast
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)
