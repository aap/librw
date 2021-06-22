add_executable(ska2anm
    ska2anm.cpp
)

target_link_libraries(ska2anm
    PUBLIC
        librw
)

librw_platform_target(ska2anm)
