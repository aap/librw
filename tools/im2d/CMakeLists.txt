add_executable(im2d WIN32
    im2d.cpp
    linelist.cpp
    main.cpp
    polyline.cpp
    trifan.cpp
    trilist.cpp
    tristrip.cpp
)

target_link_libraries(im2d
    PRIVATE
        librw::skeleton
        librw::librw
)

add_custom_command(
    TARGET im2d POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:im2d>/files"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory  "${CMAKE_CURRENT_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:im2d>/files"
)

librw_platform_target(im2d)
