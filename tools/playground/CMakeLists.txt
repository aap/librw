add_executable(playgound
    camera.cpp font.cpp main.cpp ras_test.cpp splines.cpp tl_tests.cpp
)

target_link_libraries(playgound
    PUBLIC
        librw librw_skeleton
)
