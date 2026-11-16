# The platform layer, which plays the N64's hardware on the host: the ROM,
# the controllers, the save memory and the window that shows the screen.
# The program's main is the executable's own, in main.c.

add_library(mg64_platform STATIC
    input.c
    input_script.c
    presenter.c
    rom_image.c
    save_memory.c
)
target_include_directories(mg64_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mg64_platform
    PUBLIC mg64_port_internal SDL3::SDL3
    PRIVATE mg64_gpu mg64_ultra mg64_port_warnings
)
