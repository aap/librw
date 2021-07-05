add_library(librw_skeleton
    glfw.cpp
    sdl2.cpp
    skeleton.cpp
    skeleton.h
    win.cpp

    imgui/imconfig.h
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui.h
    imgui/imgui_impl_rw.cpp
    imgui/imgui_impl_rw.h
    imgui/imgui_internal.h
    imgui/ImGuizmo.cpp
    imgui/ImGuizmo.h
    imgui/stb_rect_pack.h
    imgui/stb_textedit.h
    imgui/stb_truetype.h
)

set_target_properties(librw_skeleton
    PROPERTIES
        PREFIX ""
        EXPORT_NAME skeleton
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

# HACK: When building with MinGW, it's necessary to link to libmingw32.a *before* the library
# that provides WinMain. To work around this, an intermediate target is created when using MinGW
# that specifies the correct linking order.
#
# TODO: Use SDL_main on platforms that require it
if(MINGW AND LIBRW_PLATFORM_D3D9)
    add_library(librw_skeleton_main INTERFACE)
    target_link_libraries(librw_skeleton_main INTERFACE -lmingw32 librw_skeleton)
    add_library(librw::skeleton ALIAS librw_skeleton_main)
else()
    add_library(librw::skeleton ALIAS librw_skeleton)
endif()

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
            imgui/stb_textedit.h
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
