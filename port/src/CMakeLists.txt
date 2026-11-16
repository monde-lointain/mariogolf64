# The game's code, the libraries that stand in for the N64's, and the program.

# The game's code, warned about only where a 64-bit host would go wrong: a
# pointer cut to 32 bits, or a number taken for a pointer
add_library(mg64_game_options INTERFACE)
target_link_libraries(mg64_game_options INTERFACE mg64_headers)
target_compile_options(mg64_game_options INTERFACE
    -Wpointer-to-int-cast
    -Wint-to-pointer-cast
    -Wno-format
    # GCC 14 made these errors, but the game's C89 code relies on them
    -Wno-error=implicit-function-declaration
    -Wno-error=incompatible-pointer-types
    -Wno-error=int-conversion
)

set(main_sources ${MG64_GAME_SOURCES})
foreach(overlay IN LISTS MG64_OVERLAY_NAMES)
    list(REMOVE_ITEM main_sources ${MG64_OVERLAY_${overlay}_SOURCES})
endforeach()

add_library(mg64_game_main OBJECT ${main_sources})
target_link_libraries(mg64_game_main PRIVATE mg64_game_options)

# Each overlay is a library of its own, so that the linker can gather its
# variables where osPcResetSegment finds them
set(overlay_targets "")
foreach(overlay IN LISTS MG64_OVERLAY_NAMES)
    add_library(mg64_${overlay} OBJECT ${MG64_OVERLAY_${overlay}_SOURCES})
    target_link_libraries(mg64_${overlay} PRIVATE mg64_game_options)
    list(APPEND overlay_targets mg64_${overlay})
endforeach()

include(OverlaySections)
mg64_write_overlay_sections(
    OVERLAYS ${MG64_OVERLAY_NAMES}
    LINKER_SCRIPT overlay_linker_script
    SOURCE overlay_sections_source
)

add_subdirectory(os)
add_subdirectory(gu)
add_subdirectory(nusys)
add_subdirectory(mus)
add_subdirectory(gfx)
add_subdirectory(platform)

add_executable(mariogolf64 "${overlay_sections_source}")
target_link_libraries(mariogolf64 PRIVATE
    mg64_headers
    mg64_game_main
    ${overlay_targets}
    mg64_platform
    mg64_nusys
    mg64_mus
    mg64_gpu
    mg64_gfx
    mg64_ultra
    mg64_gu
    SDL3::SDL3
)
target_link_options(mariogolf64 PRIVATE "LINKER:-T,${overlay_linker_script}")
set_property(TARGET mariogolf64 APPEND PROPERTY LINK_DEPENDS "${overlay_linker_script}")
if(NOT WIN32)
    target_link_libraries(mariogolf64 PRIVATE m)
endif()
