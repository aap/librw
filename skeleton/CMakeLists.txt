add_library(librw_skeleton
    glfw.cpp
    sdl2.cpp
    skeleton.cpp
    skeleton.h
    win.cpp

    imgui/imgui_impl_rw.cpp
    imgui/imgui_impl_rw.h

    imgui/imconfig.h
    imgui/imgui.cpp
    imgui/imgui.h
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_internal.h
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imstb_rectpack.h
    imgui/imstb_textedit.h
    imgui/imstb_truetype.h

    imgui/ImGuizmo.cpp
    imgui/ImGuizmo.h
)
add_library(librw::skeleton ALIAS librw_skeleton)

set_target_properties(librw_skeleton
    PROPERTIES
        PREFIX ""
        EXPORT_NAME skeleton
        CXX_STANDARD 11
)

target_link_libraries(librw_skeleton
    PRIVATE
        librw
)

target_include_directories(librw_skeleton
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:${LIBRW_INSTALL_INCLUDEDIR}/skeleton>
)

if(LIBRW_INSTALL)
    install(
        FILES
            skeleton.h
        DESTINATION "${LIBRW_INSTALL_INCLUDEDIR}/skeleton"
    )

    install(
        FILES
            imgui/imconfig.h
            imgui/imgui.h
            imgui/imgui_impl_rw.h
            imgui/imgui_internal.h
            imgui/ImGuizmo.h
            imgui/imstb_rectpack.h
            imgui/imstb_textedit.h
            imgui/imstb_truetype.h
        DESTINATION "${LIBRW_INSTALL_INCLUDEDIR}/skeleton/imgui"
    )

    install(
        FILES
            imgui/LICENSE_imgui.txt
            imgui/LICENSE_imguizmo.txt
        DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    )

    install(
        TARGETS librw_skeleton
        EXPORT librw-targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endif()
