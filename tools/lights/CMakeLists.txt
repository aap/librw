add_executable(lights WIN32
    main.cpp lights.cpp
)

target_link_libraries(lights
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)

add_custom_command(
    TARGET lights POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/checker.dff" "$<TARGET_FILE_DIR:lights>"
)
