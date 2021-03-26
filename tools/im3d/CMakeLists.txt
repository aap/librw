add_executable(im3d WIN32
    im3d.cpp linelist.cpp main.cpp polyline.cpp trifan.cpp trilist.cpp tristrip.cpp
)

target_link_libraries(im3d
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)

add_custom_command(
    TARGET im3d POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory  "${CMAKE_CURRENT_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:im3d>/files"
)
