# Gathers each overlay's variables where the port can reset them.
#
# On the N64 an overlay's variables took their initial values each time the
# overlay was read from ROM, and its bss was cleared. The port links every
# overlay into the program, so it keeps a copy of each overlay's variables
# from start-up, data and bss, and copies it back when the overlay loads.
#
# A linker script gathers the .data and .bss input sections of each overlay's
# object files, which CMake writes under the directory of the overlay's
# target, mg64_<overlay>.dir, into two output sections, .ovldata and .ovlbss,
# with symbols at each overlay's bounds. A generated C file lists those bounds
# for osPcResetSegment.
#
# One section of each kind, rather than one for each overlay, keeps the
# script the same for ELF and Windows' PE: PE's linker drops an empty section,
# with any symbols in it, and an overlay may have no variables of a kind. PE
# also needs each section to start at a page, where the loader maps it; its
# own sections do so through BLOCK(__section_alignment__), which ELF's linker
# lacks.
#
#   mg64_gather_overlay_variables(TARGET <program> OVERLAYS <name>...)
#
# links the program, which must link each overlay's object library,
# mg64_<name>, with the script, and builds the C file into it.

function(mg64_gather_overlay_variables)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "TARGET" "OVERLAYS")
    if(NOT arg_TARGET OR NOT arg_OVERLAYS)
        message(FATAL_ERROR "mg64_gather_overlay_variables needs TARGET and OVERLAYS")
    endif()

    if(WIN32)
        set(alignment " BLOCK(__section_alignment__)")
    else()
        set(alignment "")
    endif()
    set(data_statements "")
    set(bss_statements "")
    set(declarations "")
    set(entries "")
    foreach(overlay IN LISTS arg_OVERLAYS)
        set(prefix "mg64_${overlay}")
        string(APPEND data_statements
            "    ${prefix}_data_start = .;\n"
            "    *${prefix}.dir/*(.data .data.*)\n"
            "    ${prefix}_data_end = .;\n")
        string(APPEND bss_statements
            "    ${prefix}_bss_start = .;\n"
            "    *${prefix}.dir/*(.bss .bss.* COMMON)\n"
            "    ${prefix}_bss_end = .;\n")
        string(APPEND declarations
            "extern unsigned char ${prefix}_data_start[], ${prefix}_data_end[];\n"
            "extern unsigned char ${prefix}_bss_start[], ${prefix}_bss_end[];\n")
        string(APPEND entries
            "    {${overlay}_SEGMENT_NUMBER, ${prefix}_data_start, ${prefix}_data_end, "
            "${prefix}_bss_start, ${prefix}_bss_end},\n")
    endforeach()

    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${arg_TARGET}")
    set(script_path "${generated_dir}/overlay_sections.ld")
    file(CONFIGURE OUTPUT "${script_path}" CONTENT [=[
/* Written by port/cmake/OverlaySections.cmake; don't edit */
SECTIONS
{
  .ovldata@alignment@ : {
@data_statements@  }
}
INSERT AFTER .data;

SECTIONS
{
  .ovlbss@alignment@ : {
@bss_statements@  }
}
INSERT AFTER .bss;
]=] @ONLY)

    set(source_path "${generated_dir}/overlay_sections.c")
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

    target_sources(${arg_TARGET} PRIVATE "${source_path}")
    target_link_options(${arg_TARGET} PRIVATE "LINKER:-T,${script_path}")
    set_property(TARGET ${arg_TARGET} APPEND PROPERTY LINK_DEPENDS "${script_path}")
endfunction()
