# The port's stand-ins for the KMC C library's routines that the host's
# would answer differently

add_library(mg64_kmc STATIC math.c random.c)
target_link_libraries(mg64_kmc
    PUBLIC mg64_port_internal
    PRIVATE mg64_port_warnings
)
