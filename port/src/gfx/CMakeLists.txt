# The renderer: an interpreter for the game's F3DEX2 display lists and RDP
# commands, and the SDL_GPU backend that draws what it records.

add_library(mg64_gfx STATIC
    frame.c
    texture_cache.c
    tmem.c
)
target_include_directories(mg64_gfx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mg64_gfx
    PUBLIC mg64_port_internal SDL3::SDL3
    PRIVATE mg64_ultra mg64_port_warnings
)
