add_executable(camera
    main.cpp camexamp.cpp viewer.cpp
)

target_link_libraries(camera
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)
