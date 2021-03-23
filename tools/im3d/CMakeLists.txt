add_executable(im3d
    im3d.cpp linelist.cpp main.cpp polyline.cpp trifan.cpp trilist.cpp tristrip.cpp
)

target_link_libraries(im3d
    PUBLIC
        librw librw_skeleton librw_skeleton_imgui
)
