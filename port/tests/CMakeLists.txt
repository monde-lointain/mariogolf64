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

# Fuzz targets: each runs the inputs in its corpus, fuzz_corpus/<target>,
# as a test. With MG64_FUZZ, which needs Clang, each is a libFuzzer program
# that also fuzzes, when run with a directory to grow a corpus in; without
# it, fuzz_driver.c runs the files named, so that every build checks the
# inputs that once failed. A target compiles the code that it fuzzes, for
# libFuzzer to see what each input reaches.
option(MG64_FUZZ "Build the fuzz targets with libFuzzer" OFF)
if(MG64_FUZZ AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "MG64_FUZZ needs Clang's libFuzzer, not ${CMAKE_C_COMPILER_ID}")
endif()

function(mg64_add_fuzz_target name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    add_executable(${name} ${arg_SOURCES})
    target_link_libraries(${name} PRIVATE ${arg_LIBRARIES} mg64_port_warnings)
    if(MG64_FUZZ)
        target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
        target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    else()
        target_sources(${name} PRIVATE fuzz_driver.c)
    endif()

    file(GLOB corpus CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz_corpus/${name}/*")
    if(NOT corpus)
        message(FATAL_ERROR "${name} has no inputs in fuzz_corpus/${name}")
    endif()
    add_test(NAME ${name} COMMAND ${name} ${corpus})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

mg64_add_fuzz_target(input_script_fuzz
    SOURCES input_script_fuzz.c "${PROJECT_SOURCE_DIR}/port/src/platform/input_script.c"
    LIBRARIES mg64_port_internal SDL3::SDL3
)
target_include_directories(input_script_fuzz PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

mg64_add_fuzz_target(config_fuzz
    SOURCES config_fuzz.c
        "${PROJECT_SOURCE_DIR}/port/src/platform/config.c"
        "${PROJECT_SOURCE_DIR}/port/src/platform/pad_mapping.c"
    LIBRARIES mg64_port_internal SDL3::SDL3
)
target_include_directories(config_fuzz PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

mg64_add_fuzz_target(rom_image_fuzz
    SOURCES rom_image_fuzz.c "${PROJECT_SOURCE_DIR}/port/src/platform/rom_image.c"
    LIBRARIES SDL3::SDL3
)
target_include_directories(rom_image_fuzz PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

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

# The game's arrays as a native build fills them from the ROM
mg64_add_port_test(rom_array_test
    SOURCES rom_array_test.c
    LIBRARIES mg64_ultra SDL3::SDL3
)

mg64_add_port_test(memory_test
    SOURCES memory_test.c
    LIBRARIES mg64_ultra SDL3::SDL3
)

# The host's copies of a character model's tables, from a model built in
# the N64's layout
mg64_add_port_test(character_model_index_test
    SOURCES character_model_index_test.c
    LIBRARIES mg64_port_internal
)

mg64_add_port_test(character_tables_test
    SOURCES character_tables_test.c
    LIBRARIES mg64_game_support mg64_ultra SDL3::SDL3
)

# The game's encoded assets, decoded from an archive on a fake cartridge
mg64_add_port_test(view_projection_test
    SOURCES view_projection_test.c "${PROJECT_SOURCE_DIR}/src/camera/view_projection.c"
    LIBRARIES mg64_port_internal m
)

mg64_add_port_test(gbi_test
    SOURCES gbi_test.c
    LIBRARIES mg64_port_internal
)

mg64_add_port_test(screen_test
    SOURCES screen_test.c
    LIBRARIES mg64_port_internal
)

mg64_add_port_test(float_to_unsigned_test
    SOURCES float_to_unsigned_test.c
    LIBRARIES mg64_port_internal
)

mg64_add_port_test(menu_icon_image_test
    SOURCES menu_icon_image_test.c
    LIBRARIES mg64_port_internal
)

mg64_add_port_test(course_space_test
    SOURCES course_space_test.c
    LIBRARIES mg64_port_internal
)

mg64_add_port_test(sky_colors_overrun_test
    SOURCES sky_colors_overrun_test.c
    LIBRARIES mg64_port_internal
)

mg64_add_port_test(math_util_test
    SOURCES math_util_test.c "${PROJECT_SOURCE_DIR}/src/math/math_util.c"
    LIBRARIES mg64_port_internal
)

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
# of its own with three overlays, in place of the doubles' empty table
include(OverlaySections)
set(sections_overlays "")
foreach(overlay IN ITEMS sections_a sections_b sections_c)
    string(REPLACE "sections_" "" letter "${overlay}")
    add_library(mg64_${overlay} OBJECT overlay_sections/overlay_${letter}.c)
    target_link_libraries(mg64_${overlay} PRIVATE mg64_port_warnings)
    list(APPEND sections_overlays mg64_${overlay})
endforeach()
# A sanitizer's checks give the overlay without variables data of their own
target_compile_options(mg64_sections_b PRIVATE -fno-sanitize=all)
# With the port's snapshot of the overlays' variables, built in so that the
# test's own linker script and GetOverlaySections give it its overlays
add_executable(overlay_sections_test
    overlay_sections_test.c
    "${PROJECT_SOURCE_DIR}/port/src/os/memory.c"
)
target_include_directories(overlay_sections_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/overlay_sections"
    "${PROJECT_SOURCE_DIR}/port/src/include"
    "${PROJECT_SOURCE_DIR}/port/src/os"
)
target_link_libraries(overlay_sections_test PRIVATE
    mg64_test_support ${sections_overlays} mg64_headers mg64_port_warnings SDL3::SDL3
)
mg64_gather_overlay_variables(TARGET overlay_sections_test OVERLAYS sections_a sections_b sections_c)
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

# The decoder of the holes' LZ-compressed data, through the copy that has
# DecodePatchedLzChunk's nested function at file scope
mg64_encode_shift_jis(lz_decoder_copy "${PROJECT_SOURCE_DIR}/src/course/lz_decoder.c")
mg64_add_port_test(lz_decoder_test
    SOURCES lz_decoder_test.c ${lz_decoder_copy}
    LIBRARIES mg64_port_internal
)

mg64_encode_shift_jis(heap_copy "${PROJECT_SOURCE_DIR}/src/system/heap.c")
# The original stores the main heap's largest free size, which nothing reads
set_property(SOURCE ${heap_copy} APPEND PROPERTY COMPILE_OPTIONS
    "$<$<AND:$<C_COMPILER_ID:Clang>,$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,23>>:-Wno-unused-but-set-global>"
)
mg64_add_port_test(heap_test
    SOURCES heap_test.c ${heap_copy}
    LIBRARIES mg64_port_internal
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
# The GPU's driver and the libraries it loads, such as libdbus, keep
# memory that LeakSanitizer reports, and the driver is unloaded before it
# reports, so no suppression can name it. The sanitizers still check the
# rest.
set_tests_properties(gpu_backend_test PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)

mg64_add_port_test(shape_overlay_test
    SOURCES shape_overlay_test.c
    LIBRARIES mg64_gpu mg64_gfx SDL3::SDL3
)
# As for gpu_backend_test
set_tests_properties(shape_overlay_test PROPERTIES
    SKIP_RETURN_CODE 77
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)

mg64_add_port_test(kmc_math_test
    SOURCES kmc_math_test.c
    LIBRARIES mg64_kmc
)

mg64_add_port_test(kmc_random_test
    SOURCES kmc_random_test.c
    LIBRARIES mg64_kmc
)

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

# The fences that keep the CPU from running far ahead of the GPU
mg64_add_port_test(frames_in_flight_test
    SOURCES frames_in_flight_test.c "${PROJECT_SOURCE_DIR}/port/src/gfx/gpu/frames_in_flight.c"
    LIBRARIES SDL3::SDL3
)
target_include_directories(frames_in_flight_test PRIVATE "${PROJECT_SOURCE_DIR}/port/src/gfx/gpu")

# The wait that keeps a headless run in step with the game, with the
# scheduler's wait replaced through a link seam
mg64_add_port_test(lockstep_test
    SOURCES lockstep_test.c "${PROJECT_SOURCE_DIR}/port/src/platform/lockstep.c"
    LIBRARIES mg64_port_internal SDL3::SDL3
)
target_include_directories(lockstep_test PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

# The player's settings and the file that holds them
mg64_add_port_test(config_test
    SOURCES config_test.c
        "${PROJECT_SOURCE_DIR}/port/src/platform/config.c"
        "${PROJECT_SOURCE_DIR}/port/src/platform/pad_mapping.c"
    LIBRARIES mg64_port_internal SDL3::SDL3
)
target_include_directories(config_test PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

# The settings file on disk
mg64_add_port_test(config_file_test
    SOURCES config_file_test.c
        "${PROJECT_SOURCE_DIR}/port/src/platform/config_file.c"
        "${PROJECT_SOURCE_DIR}/port/src/platform/config.c"
        "${PROJECT_SOURCE_DIR}/port/src/platform/pad_mapping.c"
    LIBRARIES mg64_port_internal SDL3::SDL3
)
target_include_directories(config_file_test PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

# The bindings of the host's gamepads and keys to the N64 controller's
mg64_add_port_test(pad_mapping_test
    SOURCES pad_mapping_test.c "${PROJECT_SOURCE_DIR}/port/src/platform/pad_mapping.c"
    LIBRARIES mg64_port_internal SDL3::SDL3
)
target_include_directories(pad_mapping_test PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

# The touch controls' layout, and what fingers on them play
mg64_add_port_test(touch_controls_test
    SOURCES touch_controls_test.c
        "${PROJECT_SOURCE_DIR}/port/src/platform/touch_controls.c"
        "${PROJECT_SOURCE_DIR}/port/src/platform/pad_mapping.c"
    LIBRARIES mg64_port_internal SDL3::SDL3
)
target_include_directories(touch_controls_test PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

# Where a run in real time stalls, from the times of its retraces
mg64_add_port_test(stall_watch_test
    SOURCES stall_watch_test.c "${PROJECT_SOURCE_DIR}/port/src/platform/stall_watch.c"
    LIBRARIES SDL3::SDL3
)
target_include_directories(stall_watch_test PRIVATE "${PROJECT_SOURCE_DIR}/port/src/platform")

# The input recorder, and the script lines that it writes
mg64_add_port_test(input_recorder_test
    SOURCES input_recorder_test.c
    LIBRARIES mg64_platform mg64_ultra SDL3::SDL3
)

# The controllers' ports, with SDL's virtual gamepads
mg64_add_port_test(controllers_test
    SOURCES controllers_test.c
    LIBRARIES mg64_platform mg64_gpu mg64_gfx mg64_ultra SDL3::SDL3
)

mg64_add_port_test(platform_test
    SOURCES platform_test.c
    LIBRARIES mg64_platform mg64_gpu mg64_gfx mg64_ultra SDL3::SDL3
)
