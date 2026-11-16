# The port's graphics utilities: libultra's gu transforms, for the host.
# The fixed-point conversions and guNormalize are the game's own.

add_library(mg64_gu STATIC gu.c)
target_link_libraries(mg64_gu
    PUBLIC mg64_port_internal
    PRIVATE mg64_port_warnings m
)
