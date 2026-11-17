# The renderer: an interpreter for the game's F3DEX2 display lists and RDP
# commands, and the SDL_GPU backend that draws what it records.

add_library(mg64_gfx STATIC
    color_images.c
    frame.c
    rdp.c
    rsp.c
    texture_cache.c
    tmem.c
)
target_include_directories(mg64_gfx PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mg64_gfx
    PUBLIC mg64_port_internal SDL3::SDL3
    PRIVATE mg64_ultra mg64_port_warnings
)
mg64_skip_type_sanitizer(mg64_gfx)

# The backend is a library of its own, so that the interpreter's tests can
# put a double in its place
include(CompileShaders)

add_library(mg64_gpu STATIC gpu/sdl_gpu_backend.c)
target_include_directories(mg64_gpu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/gpu")
target_link_libraries(mg64_gpu
    PUBLIC mg64_gfx SDL3::SDL3
    PRIVATE mg64_ultra mg64_port_warnings
)
mg64_compile_shaders(mg64_gpu SHADERS shaders/rdp.vert shaders/rdp.frag)
