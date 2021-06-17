find_library(NXGL_EGL_LIBRARY EGL)
find_library(NXGL_GLAPI_LIBRARY glapi)
find_library(NXGL_DRM_NOUVEAU_LIBRARY drm_nouveau)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NXGL
    REQUIRED_VARS NXGL_EGL_LIBRARY NXGL_GLAPI_LIBRARY NXGL_DRM_NOUVEAU_LIBRARY
)

if(NXGL_FOUND)
    if(NOT TARGET NXGL::EGL)
        add_library(NXGL::EGL UNKNOWN IMPORTED)
        set_target_properties(NXGL::EGL PROPERTIES
            IMPORTED_LOCATION "${NXGL_EGL_LIBRARY}"
        )
    endif()

    if(NOT TARGET NXGL::glapi)
        add_library(NXGL::glapi UNKNOWN IMPORTED)
        set_target_properties(NXGL::glapi PROPERTIES
            IMPORTED_LOCATION "${NXGL_GLAPI_LIBRARY}"
        )
    endif()

    if(NOT TARGET NXGL::drm_nouveau)
        add_library(NXGL::drm_nouveau UNKNOWN IMPORTED)
        set_target_properties(NXGL::drm_nouveau PROPERTIES
            IMPORTED_LOCATION "${NXGL_DRM_NOUVEAU_LIBRARY}"
        )
    endif()

    if(NOT TARGET NXGL::OpenGL)
        add_library(NXGL::OpenGL INTERFACE IMPORTED)
        set_target_properties(NXGL::OpenGL PROPERTIES
            INTERFACE_LINK_LIBRARIES "NXGL::EGL;NXGL::glapi;NXGL::drm_nouveau"
        )
    endif()
endif()
