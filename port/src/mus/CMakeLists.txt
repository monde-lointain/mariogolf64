# The port's music player, which stands in for libmus: the synthesizer that
# plays the sample bank's sounds, and the sequencer that drives it from the
# game's songs and sound effects.

add_library(mg64_synth STATIC
    reverb.c
    sequencer.c
    sound_data.c
    synth.c
    vadpcm.c
)
target_include_directories(mg64_synth PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mg64_synth
    PUBLIC mg64_port_internal
    PRIVATE SDL3::SDL3 mg64_port_warnings
)
mg64_skip_type_sanitizer(mg64_synth)

# The libmus API over the synthesizer. It plays into the audio output that
# audio_output.h declares, which the program links from mg64_audio_output and
# the tests from their own doubles.
add_library(mg64_mus STATIC music_player.c)
target_link_libraries(mg64_mus
    PUBLIC mg64_synth
    PRIVATE mg64_ultra SDL3::SDL3 mg64_port_warnings
)

add_library(mg64_audio_output STATIC sdl_audio_output.c)
target_link_libraries(mg64_audio_output
    PUBLIC mg64_port_internal
    PRIVATE SDL3::SDL3 mg64_port_warnings
)
