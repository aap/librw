add_executable(subrast WIN32
    main.cpp
    subrast.cpp
    subrast.h
)

target_link_libraries(subrast
    PUBLIC
        librw::skeleton
        librw::librw
)

add_custom_command(
    TARGET subrast POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:subrast>/files"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory  "${CMAKE_CURRENT_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:subrast>/files"
)

librw_platform_target(subrast)
