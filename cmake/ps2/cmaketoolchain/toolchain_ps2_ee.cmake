cmake_minimum_required(VERSION 3.7)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_SYSTEM_NAME "PlayStation2")
set(CMAKE_SYSTEM_PROCESSOR "mips64r5900el")
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED ENV{PS2DEV})
    message(FATAL_ERROR "Need environment variable PS2DEV set")
endif()
if(NOT DEFINED ENV{PS2SDK})
    message(FATAL_ERROR "Need environment variable PS2SDK set")
endif()
if(NOT DEFINED ENV{GSKIT})
    message(FATAL_ERROR "Need environment variable PS2SDK set")
endif()

set(PS2DEV "$ENV{PS2DEV}")
set(PS2SDK "$ENV{PS2SDK}")
set(GSKIT "$ENV{GSKIT}")

if(NOT IS_DIRECTORY "${PS2DEV}")
    message(FATAL_ERROR "PS2DEV must be a folder path (${PS2DEV})")
endif()

if(NOT IS_DIRECTORY "${PS2SDK}")
    message(FATAL_ERROR "PS2SDK must be a folder path (${PS2SDK})")
endif()

if(NOT IS_DIRECTORY "${GSKIT}")
    message(FATAL_ERROR "GSKIT must be a folder path (${GSKIT})")
endif()

set(CMAKE_DSM_SOURCE_FILE_EXTENSIONS "dsm")

set(CMAKE_C_COMPILER "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-gcc" CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-g++" CACHE FILEPATH "CXX compiler")
set(CMAKE_ASM_COMPILER "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-g++" CACHE FILEPATH "ASM assembler")
set(CMAKE_DSM_COMPILER "${PS2DEV}/dvp/bin/dvp-as" CACHE FILEPATH "DSM assembler")
set(CMAKE_AR "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-ar" CACHE FILEPATH "archiver")
set(CMAKE_LINKER "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-ld" CACHE FILEPATH "Linker")
set(CMAKE_RANLIB "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-ranlib" CACHE FILEPATH "ranlib")
set(CMAKE_STRIP "${PS2DEV}/ee/bin/mips64r5900el-ps2-elf-strip" CACHE FILEPATH "strip")

set(CMAKE_ASM_FLAGS_INIT "-G0 -I\"${PS2SDK}/ee/include\" -I\"${PS2SDK}/common/include\" -D_EE")
set(CMAKE_C_FLAGS_INIT "-G0 -fno-common -I\"${PS2SDK}/ee/include\" -I\"${PS2SDK}/common/include\" -D_EE")
set(CMAKE_CXX_FLAGS_INIT "-G0 -fno-common -I\"${PS2SDK}/ee/include\" -I\"${PS2SDK}/common/include\" -D_EE")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-G0 -L\"${PS2SDK}/ee/lib\" -Wl,-r -Wl,-d")

set(CMAKE_FIND_ROOT_PATH "${PS2DEV}/ee" "${PS2SDK}/ee" "${GSKIT}")
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(PS2 1)
set(EE 1)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

function(add_erl_executable TARGET)
    cmake_parse_arguments("AEE" "" "OUTPUT_VAR" "" ${ARGN})

    get_target_property(output_dir "${TARGET}" RUNTIME_OUTPUT_DIRECTORY)
    if(NOT output_dir)
        set(output_dir ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    get_target_property(output_name ${TARGET} OUTPUT_NAME)
    if(NOT output_name)
        set(output_name ${TARGET})
    endif()
    set(outfile "${output_dir}/${output_name}.erl")

    add_custom_command(OUTPUT "${outfile}"
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${TARGET}>" "${outfile}"
        COMMAND "${CMAKE_STRIP}" --strip-unneeded -R .mdebug.eabi64 -R .reginfo -R .comment "${outfile}"
        DEPENDS ${TARGET}
    )
    add_custom_target("${TARGET}_erl" ALL
        DEPENDS "${outfile}"
    )

    if(AEE_OUTPUT_VAR)
        set("${AEE_OUTPUT_VAR}" "${outfile}" PARENT_SCOPE)
    endif()
endfunction()
