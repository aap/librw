from conans import ConanFile, CMake, tools
from conans.errors import ConanInvalidConfiguration
import os
import shutil
import textwrap


class LibrwConan(ConanFile):
    name = "librw"
    version = "master"
    license = "MIT"
    settings = "os", "arch", "compiler", "build_type"
    generators = "cmake", "cmake_find_package"
    options = {
        "platform": ["null", "gl3", "d3d9", "ps2"],
        "gl3_gfxlib": ["glfw", "sdl2"],
    }
    default_options = {
        "platform": "gl3",
        "gl3_gfxlib": "glfw",
        "sdl2:vulkan": False,
        "sdl2:opengl": True,
        "sdl2:sdl2main": False,
    }
    no_copy_source = True

    def config_options(self):
        if self.settings.os == "Windows":
            self.options["sdl2"].directx = False

    def configure(self):
        if self.options.platform != "gl3":
            del self.options.gl3_gfxlib
        if self.options.platform == "d3d9" and self.settings.os != "Windows":
            raise ConanInvalidConfiguration("d3d9 can only be built for Windows")

    def requirements(self):
        if self.options.platform == "gl3":
            self.requires("glew/2.1.0")
            if self.options.gl3_gfxlib == "glfw":
                self.requires("glfw/3.3.2")
            elif self.options.gl3_gfxlib == "sdl2":
                self.requires("sdl2/2.0.12@bincrafters/stable")

    def export_sources(self):
        for d in ("cmake", "skeleton", "src", "tools"):
            shutil.copytree(src=d, dst=os.path.join(self.export_sources_folder, d))
        self.copy("args.h")
        self.copy("rw.h")
        self.copy("CMakeLists.txt")
        self.copy("LICENSE")

    @property
    def _librw_platform(self):
        return {
            "null": "NULL",
            "gl3": "GL3",
            "d3d9": "D3D9",
            "ps2": "PS2",
        }[str(self.options.platform)]

    def build(self):
        if self.source_folder == self.build_folder:
            raise Exception("cannot build with source_folder == build_folder")
        tools.save("Findglfw3.cmake",
                   textwrap.dedent(
                       """
                       if(NOT TARGET glfw)
                         message(STATUS "Creating glfw TARGET")
                         add_library(glfw INTERFACE IMPORTED)
                         set_target_properties(glfw PROPERTIES
                            INTERFACE_LINK_LIBRARIES CONAN_PKG::glfw)  #$<BUILD_INTERFACE:CONAN_PKG::glfw>)
                       endif()
                       """), append=True)
        tools.save("CMakeLists.txt",
                   textwrap.dedent(
                       """
                       cmake_minimum_required(VERSION 3.0)
                       project(cmake_wrapper)

                       include("{}/conanbuildinfo.cmake")
                       conan_basic_setup(TARGETS)

                       add_subdirectory("{}" librw)
                       """).format(self.install_folder.replace("\\", "/"),
                                   self.source_folder.replace("\\", "/")))
        cmake = CMake(self)
        cmake.definitions["LIBRW_PLATFORM"] = self._librw_platform
        cmake.definitions["LIBRW_INSTALL"] = True
        if self.options.platform == "gl3":
            cmake.definitions["LIBRW_GL3_GFXLIB"] = str(self.options.gl3_gfxlib).upper()
        cmake.configure(source_folder=self.build_folder)
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.includedirs.append(os.path.join("include", "librw"))
        self.cpp_info.libs = ["librw"]
        if self.options.platform == "null":
            self.cpp_info.defines.append("RW_NULL")
        elif self.options.platform == "gl3":
            self.cpp_info.defines.append("RW_GL3")
            if self.options.gl3_gfxlib == "sdl2":
                self.cpp_info.defines.append("LIBRW_SDL2")
        elif self.options.platform == "d3d9":
            self.cpp_info.defines.append("RW_D3D9")
        elif self.options.platform == "ps2":
            self.cpp_info.defines.append("RW_PS2")
