# The platform layer, which plays the N64's hardware on the host: the ROM,
# the controllers, the save memory and the window that shows the screen.
# The program's main is the executable's own, in main.c.

# The platform's headers alone, for the tests that build its sources one by
# one, in place of the library, to put doubles beside them
add_library(mg64_platform_headers INTERFACE)
target_include_directories(mg64_platform_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(mg64_platform STATIC
    config.c
    config_file.c
    input.c
    input_recorder.c
    input_script.c
    lockstep.c
    options.c
    pad_mapping.c
    presenter.c
    rom_image.c
    save_memory.c
    stall_watch.c
    touch_controls.c
    window_place.c
    "${CMAKE_CURRENT_BINARY_DIR}/version.c"
)

# The version and revision, written at each build, since a commit changes
# the revision without CMake knowing
add_custom_target(mg64_version
    COMMAND "${CMAKE_COMMAND}"
        "-DVERSION=${PROJECT_VERSION}"
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/version.c"
        -P "${PROJECT_SOURCE_DIR}/port/cmake/WriteVersion.cmake"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/version.c"
    COMMENT "Writing the version"
    VERBATIM
)
add_dependencies(mg64_platform mg64_version)
target_link_libraries(mg64_platform
    PUBLIC mg64_platform_headers mg64_port_internal SDL3::SDL3
    PRIVATE mg64_gpu mg64_ultra mg64_port_warnings
)
