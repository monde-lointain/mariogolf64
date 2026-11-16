# The port's NuSystem: the scheduler, the graphics tasks and frame buffers,
# the controllers, and the cartridge's ROM and SRAM, on the port's libultra

add_library(mg64_nusys STATIC
    boot.c
    controllers.c
    graphics.c
    pi.c
    scheduler.c
)
target_link_libraries(mg64_nusys
    PUBLIC mg64_port_internal
    PRIVATE mg64_ultra mg64_port_warnings
)
