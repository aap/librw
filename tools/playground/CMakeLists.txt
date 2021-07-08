add_executable(playground WIN32
    camera.cpp
    font.cpp
    main.cpp
    ras_test.cpp
    splines.cpp
    tl_tests.cpp
)

target_link_libraries(playground
    PRIVATE
        librw::skeleton
        librw::librw
)

add_custom_command(
    TARGET playground POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:playground>/files"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory  "${CMAKE_CURRENT_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:playground>/files"
)

librw_platform_target(playground)
