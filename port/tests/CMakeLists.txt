# Tests of the port's own libraries, run on the host with CTest. They link
# the libraries alone, with test doubles for the program's parts that the
# libraries call.

add_library(mg64_test_support STATIC test_support.c)
target_include_directories(mg64_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mg64_test_support PRIVATE m)

# Doubles for the program's parts that the port's libraries call. Their
# objects go into every test, wherever the libraries that call them fall
# on the link line.
add_library(mg64_test_doubles OBJECT no_overlays.c)
target_link_libraries(mg64_test_doubles PRIVATE mg64_port_internal)

function(mg64_add_port_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name}
        PRIVATE mg64_test_support ${arg_LIBRARIES} mg64_test_doubles mg64_port_warnings
    )
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

mg64_add_port_test(os_threads_test
    SOURCES os_threads_test.c
    LIBRARIES mg64_ultra SDL3::SDL3
)

# The game's own fixed-point conversions, which the gu routines call
mg64_add_port_test(gu_test
    SOURCES gu_test.c "${PROJECT_SOURCE_DIR}/src/math/matrix_conversion.c"
    LIBRARIES mg64_gu
)

mg64_add_port_test(frame_test
    SOURCES frame_test.c
    LIBRARIES mg64_gfx SDL3::SDL3
)

mg64_add_port_test(nusys_test
    SOURCES nusys_test.c
    LIBRARIES mg64_nusys mg64_ultra SDL3::SDL3
)

mg64_add_port_test(rom_order_test
    SOURCES rom_order_test.c
    LIBRARIES mg64_ultra SDL3::SDL3
)

mg64_add_port_test(memory_test
    SOURCES memory_test.c
    LIBRARIES mg64_ultra SDL3::SDL3
)

# The host's copies of a character model's tables, from a model built in
# the N64's layout
mg64_add_port_test(character_tables_test
    SOURCES character_tables_test.c
    LIBRARIES mg64_game_support mg64_ultra SDL3::SDL3
)

# The game's encoded assets, decoded from an archive on a fake cartridge
mg64_add_port_test(asset_test
    SOURCES asset_test.c "${PROJECT_SOURCE_DIR}/src/system/asset.c"
    LIBRARIES mg64_ultra SDL3::SDL3
)

# The debug font as the CPU and the RDP each read it
mg64_add_port_test(debug_text_test
    SOURCES debug_text_test.c "${PROJECT_SOURCE_DIR}/src/debug/debug_text.c"
    LIBRARIES mg64_game_options mg64_ultra SDL3::SDL3
)

# The message file as the CPU reads it
mg64_add_port_test(text_test
    SOURCES text_test.c "${PROJECT_SOURCE_DIR}/src/text/text.c"
    LIBRARIES mg64_game_options mg64_ultra SDL3::SDL3
)

# The performance meter's digits as the RDP loads them
mg64_add_port_test(performance_meter_test
    SOURCES performance_meter_test.c "${PROJECT_SOURCE_DIR}/src/debug/performance_meter.c"
    LIBRARIES mg64_game_options mg64_ultra SDL3::SDL3
)

# The move of the game's nested functions to file scope, whose output it
# compiles and runs with the build's C compiler, through the emulator that
# runs the other tests when cross-compiling. Python is named by its path, as
# CTest would run an executable target, even an imported one, through the
# emulator too.
set(emulator_arguments "")
foreach(word IN LISTS CMAKE_CROSSCOMPILING_EMULATOR)
    list(APPEND emulator_arguments "--emulator=${word}")
endforeach()
add_test(NAME nested_functions_test
    COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/nested_functions_test.py"
        --compiler "${CMAKE_C_COMPILER}"
        "--executable-suffix=${CMAKE_EXECUTABLE_SUFFIX}"
        ${emulator_arguments}
)

# The linker script that gathers overlays' variables, linked into a program
# of its own with two overlays, in place of the doubles' empty table
include(OverlaySections)
set(sections_overlays "")
foreach(overlay IN ITEMS sections_a sections_b)
    string(REPLACE "sections_" "" letter "${overlay}")
    add_library(mg64_${overlay} OBJECT overlay_sections/overlay_${letter}.c)
    target_link_libraries(mg64_${overlay} PRIVATE mg64_port_warnings)
    list(APPEND sections_overlays mg64_${overlay})
endforeach()
mg64_write_overlay_sections(
    OVERLAYS sections_a sections_b
    LINKER_SCRIPT sections_linker_script
    SOURCE sections_source
)
add_executable(overlay_sections_test overlay_sections_test.c "${sections_source}")
target_include_directories(overlay_sections_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/overlay_sections"
    "${PROJECT_SOURCE_DIR}/port/src/include"
)
target_link_libraries(overlay_sections_test PRIVATE
    mg64_test_support ${sections_overlays} mg64_port_warnings
)
target_link_options(overlay_sections_test PRIVATE "LINKER:-T,${sections_linker_script}")
set_property(TARGET overlay_sections_test APPEND PROPERTY LINK_DEPENDS "${sections_linker_script}")
add_test(NAME overlay_sections_test COMMAND overlay_sections_test)
set_tests_properties(overlay_sections_test PROPERTIES TIMEOUT 60)

# Built through a Shift-JIS copy, as the game's sources are
include(ShiftJisSources)
mg64_encode_shift_jis(shift_jis_test_copy "${CMAKE_CURRENT_SOURCE_DIR}/shift_jis_test.c")
mg64_add_port_test(shift_jis_test
    SOURCES ${shift_jis_test_copy}
)
target_compile_definitions(shift_jis_test PRIVATE
    SHIFT_JIS_TEST_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/shift_jis_test.c"
)

mg64_add_port_test(tmem_test
    SOURCES tmem_test.c
    LIBRARIES mg64_gfx
)

mg64_add_port_test(texture_cache_test
    SOURCES texture_cache_test.c
    LIBRARIES mg64_gfx SDL3::SDL3
)

# The interpreter alone, with a double of the GPU backend
mg64_add_port_test(rcp_test
    SOURCES rcp_test.c
    LIBRARIES mg64_gfx SDL3::SDL3
)

# The renderer on a real GPU, which the test skips when there is none
mg64_add_port_test(gpu_backend_test
    SOURCES gpu_backend_test.c
    LIBRARIES mg64_gpu mg64_gfx SDL3::SDL3
)
set_tests_properties(gpu_backend_test PROPERTIES SKIP_RETURN_CODE 77)

mg64_add_port_test(vadpcm_test
    SOURCES vadpcm_test.c
    LIBRARIES mg64_synth
)

mg64_add_port_test(sound_data_test
    SOURCES sound_data_test.c
    LIBRARIES mg64_synth
)

mg64_add_port_test(synth_test
    SOURCES synth_test.c
    LIBRARIES mg64_synth SDL3::SDL3
)

mg64_add_port_test(sequencer_test
    SOURCES sequencer_test.c
    LIBRARIES mg64_synth SDL3::SDL3
)

# The player's thread runs on the port's libultra; the audio output is the
# test's own
mg64_add_port_test(music_player_test
    SOURCES music_player_test.c
    LIBRARIES mg64_mus mg64_ultra SDL3::SDL3
)

# The platform's parts that need no window or device. The presenter's
# object comes along, so the renderer's libraries do too.
# Scripted input's parser and lookup
mg64_add_port_test(input_script_test
    SOURCES input_script_test.c
    LIBRARIES mg64_platform mg64_ultra SDL3::SDL3
)

mg64_add_port_test(platform_test
    SOURCES platform_test.c
    LIBRARIES mg64_platform mg64_gpu mg64_gfx mg64_ultra SDL3::SDL3
)
