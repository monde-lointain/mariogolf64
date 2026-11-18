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
    # Clang warns at what the matched code does on purpose: calls to
    # GetCourseRecordIndex, left unprototyped (see course.h); fabsf taken of
    # integers, which it converts as the game meant; and the notice's float
    # compared with the double M_PI (see DrawCourseResultsNotice)
    $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-deprecated-non-prototype>
    $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-absolute-value>
    $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-literal-range>
    # The matched code calls sinf and cosf for nothing, as the original
    # did, which Clang notices where the C library's maths sets no errno,
    # as Android's doesn't
    $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-unused-value>
)
# The KMC C library's random numbers and trigonometry, in place of the
# host's, which differ from them and from each other (see port/kmc_names.h)
target_compile_options(mg64_game_options INTERFACE
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/include/port/kmc_names.h\""
)
target_link_libraries(mg64_game_options INTERFACE mg64_kmc)

set(main_sources ${MG64_GAME_SOURCES})
foreach(overlay IN LISTS MG64_OVERLAY_NAMES)
    list(REMOVE_ITEM main_sources ${MG64_OVERLAY_${overlay}_SOURCES})
endforeach()

include(ShiftJisSources)
mg64_encode_shift_jis(main_copies ${main_sources})
add_library(mg64_game_main OBJECT ${main_copies})
target_link_libraries(mg64_game_main PRIVATE mg64_game_options)
mg64_skip_pointer_pair_checks(mg64_game_main src/math/math_util.c src/game/game.c)

# Each overlay is a library of its own, so that the linker can gather its
# variables where osPcResetSegment finds them
set(overlay_targets "")
foreach(overlay IN LISTS MG64_OVERLAY_NAMES)
    mg64_encode_shift_jis(overlay_copies ${MG64_OVERLAY_${overlay}_SOURCES})
    add_library(mg64_${overlay} OBJECT ${overlay_copies})
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
add_subdirectory(kmc)
add_subdirectory(nusys)
add_subdirectory(mus)
add_subdirectory(gfx)
add_subdirectory(game)
add_subdirectory(platform)

add_executable(mariogolf64 platform/main.c "${overlay_sections_source}")
target_link_libraries(mariogolf64 PRIVATE
    mg64_headers
    mg64_game_main
    ${overlay_targets}
    mg64_game_support
    mg64_platform
    mg64_nusys
    mg64_mus
    mg64_audio_output
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
