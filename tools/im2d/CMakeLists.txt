add_executable(im2d
    im2d.cpp linelist.cpp main.cpp polyline.cpp trifan.cpp trilist.cpp tristrip.cpp
)

target_link_libraries(im2d
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)
