# The port's libultra: threads on the host's threads, message queues, the
# video interface, DMA from the cartridge, the TLB and the clock.

add_library(mg64_ultra STATIC
    clock.c
    init.c
    memory.c
    message.c
    pi.c
    scheduler.c
    vi.c
)
target_link_libraries(mg64_ultra
    PUBLIC mg64_port_internal
    PRIVATE SDL3::SDL3 mg64_port_warnings
)
