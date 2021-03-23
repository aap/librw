add_executable(lights
    main.cpp lights.cpp
)

target_link_libraries(lights
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)
