# The port's music player, which stands in for libmus: the synthesizer that
# plays the sample bank's sounds, and the sequencer that drives it from the
# game's songs and sound effects.

add_library(mg64_synth STATIC
    reverb.c
    sound_data.c
    synth.c
    vadpcm.c
)
target_include_directories(mg64_synth PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mg64_synth
    PUBLIC mg64_port_internal
    PRIVATE SDL3::SDL3 mg64_port_warnings
)

add_library(mg64_mus INTERFACE)
target_link_libraries(mg64_mus INTERFACE mg64_port_internal)
