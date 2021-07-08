if(CMAKE_C_COMPILER_ID MATCHES "^MSVC$")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()
