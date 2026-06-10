Build with cmake
================

Linux

   mkdir build
   cd build
   cmake .. -DLIBRW_PLATFORM=GL3 -DLIBRW_GL3_GFXLIB=SDL2
   make

Android NDK + SDL3 + Vulkan

   cmake -S . -B build-android-vulkan -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-24 \
      -DLIBRW_PLATFORM=VULKAN \
      -DLIBRW_TOOLS=OFF \
      -DLIBRW_INSTALL=OFF \
      -DLIBRW_SDL3_SOURCE_DIR=/path/to/SDL3
   cmake --build build-android-vulkan --target librw

If the parent Android project already defines SDL3::SDL3, librw will reuse
that target. Otherwise LIBRW_SDL3_SOURCE_DIR can point at an SDL3 source tree.
