add_executable(camera WIN32
    main.cpp
    camexamp.cpp
    viewer.cpp
)

target_link_libraries(camera
    PRIVATE
        librw::skeleton
        librw::librw
)

add_custom_command(
    TARGET camera POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:camera>/files"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:camera>/files"
)

librw_platform_target(camera)
