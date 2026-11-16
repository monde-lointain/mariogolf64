# What the port adds to the game's own code: the host's copies of data that
# the game reads in the N64's layout.

add_library(mg64_game_support STATIC
    character_tables.c
)
target_link_libraries(mg64_game_support
    PUBLIC mg64_port_internal
    PRIVATE SDL3::SDL3 mg64_ultra mg64_port_warnings
)
