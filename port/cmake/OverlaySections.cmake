# Gathers each overlay's variables where the port can reset them.
#
# On the N64 an overlay's variables took their initial values each time the
# overlay was read from ROM, and its bss was cleared. The port links every
# overlay into the program, so it keeps a copy of each overlay's initialized
# variables from start-up and copies it back when the overlay loads.
#
# A linker script gathers the .data and .bss input sections of each overlay's
# object files, which CMake writes under the directory of the overlay's
# target, mg64_<overlay>.dir, into output sections of their own, bounded by
# symbols. A generated C file lists those bounds for osPcResetSegment.
#
#   mg64_write_overlay_sections(OVERLAYS <name>...
#                               LINKER_SCRIPT <variable> SOURCE <variable>)
#
# sets the variables to the paths of the linker script and the C file.

function(mg64_write_overlay_sections)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "LINKER_SCRIPT;SOURCE" "OVERLAYS")
    if(NOT arg_OVERLAYS OR NOT arg_LINKER_SCRIPT OR NOT arg_SOURCE)
        message(FATAL_ERROR "mg64_write_overlay_sections needs OVERLAYS, LINKER_SCRIPT and SOURCE")
    endif()

    set(data_statements "")
    set(bss_statements "")
    set(declarations "")
    set(entries "")
    foreach(overlay IN LISTS arg_OVERLAYS)
        set(prefix "mg64_${overlay}")
        string(APPEND data_statements
            "  .${prefix}_data : {\n"
            "    ${prefix}_data_start = .;\n"
            "    *${prefix}.dir/*(.data .data.*)\n"
            "    ${prefix}_data_end = .;\n"
            "  }\n")
        string(APPEND bss_statements
            "  .${prefix}_bss (NOLOAD) : {\n"
            "    ${prefix}_bss_start = .;\n"
            "    *${prefix}.dir/*(.bss .bss.* COMMON)\n"
            "    ${prefix}_bss_end = .;\n"
            "  }\n")
        string(APPEND declarations
            "extern unsigned char ${prefix}_data_start[], ${prefix}_data_end[];\n"
            "extern unsigned char ${prefix}_bss_start[], ${prefix}_bss_end[];\n")
        string(APPEND entries
            "    {${overlay}_SEGMENT_NUMBER, ${prefix}_data_start, ${prefix}_data_end, "
            "${prefix}_bss_start, ${prefix}_bss_end},\n")
    endforeach()

    set(script_path "${CMAKE_CURRENT_BINARY_DIR}/generated/overlay_sections.ld")
    file(CONFIGURE OUTPUT "${script_path}" CONTENT [=[
/* Written by port/cmake/OverlaySections.cmake; don't edit */
SECTIONS
{
@data_statements@}
INSERT AFTER .data;

SECTIONS
{
@bss_statements@}
INSERT AFTER .bss;
]=] @ONLY)

    set(source_path "${CMAKE_CURRENT_BINARY_DIR}/generated/overlay_sections.c")
    file(CONFIGURE OUTPUT "${source_path}" CONTENT [=[
/* Written by port/cmake/OverlaySections.cmake; don't edit */
#include <rom_file_offsets.h>

#include "port/overlay_sections.h"

@declarations@
static const OverlaySections f_OverlaySections[] = {
@entries@};

const OverlaySections *GetOverlaySections(int *count) {
    *count = (int)(sizeof(f_OverlaySections) / sizeof(f_OverlaySections[0]));
    return f_OverlaySections;
}
]=] @ONLY)

    set(${arg_LINKER_SCRIPT} "${script_path}" PARENT_SCOPE)
    set(${arg_SOURCE} "${source_path}" PARENT_SCOPE)
endfunction()
