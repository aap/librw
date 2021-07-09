add_executable(lights WIN32
    main.cpp
    lights.cpp
)

target_link_libraries(lights
    PRIVATE
        librw::skeleton
        librw::librw
)

add_custom_command(
    TARGET lights POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:lights>/files"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/checker.dff" "$<TARGET_FILE_DIR:lights>"
)

librw_platform_target(lights)
