add_executable(playgound WIN32
    camera.cpp font.cpp main.cpp ras_test.cpp splines.cpp tl_tests.cpp
)

target_link_libraries(playgound
    PUBLIC
        librw librw_skeleton
)

add_custom_command(
    TARGET playgound POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/teapot.dff" "${CMAKE_CURRENT_SOURCE_DIR}/maze.tga" "${CMAKE_CURRENT_SOURCE_DIR}/Bm437_IBM_VGA8.FON" "${CMAKE_CURRENT_SOURCE_DIR}/Bm437_IBM_BIOS.FON" "$<TARGET_FILE_DIR:playground>"
)
