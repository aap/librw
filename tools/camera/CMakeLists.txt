add_executable(camera WIN32
    main.cpp camexamp.cpp viewer.cpp
)

target_link_libraries(camera
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)

add_custom_command(
    TARGET camera POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory  "${CMAKE_CURRENT_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:camera>/files"
)

librw_platform_target(camera)
