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
# Apple's linker takes no script. There each overlay's sources are compiled
# with a forced include whose #pragma clang section puts their variables in
# sections of the overlay's own, __DATA,__ovd_<overlay> and a zero-filled
# __DATA,__ovb_<overlay>. The linker gives each section's bounds to the
# symbols section$start$<segment>$<section> and section$end$..., and to
# valid, equal bounds for a section that no object has.
#
#   mg64_gather_overlay_variables(TARGET <program> OVERLAYS <name>...)
#
# links the program, which must link each overlay's object library,
# mg64_<name>, with the script, and builds the C file into it. On Apple's
# platforms it compiles each overlay's library with the overlay's pragma.

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
    set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${arg_TARGET}")
    set(data_statements "")
    set(bss_statements "")
    set(declarations "")
    set(entries "")
    foreach(overlay IN LISTS arg_OVERLAYS)
        set(prefix "mg64_${overlay}")
        if(APPLE)
            set(data_section "__ovd_${overlay}")
            set(bss_section "__ovb_${overlay}")
            # Mach-O holds 16 characters of a section's name
            string(LENGTH "${data_section}" name_length)
            if(name_length GREATER 16)
                message(FATAL_ERROR
                    "The overlay ${overlay}'s name is too long for a Mach-O section")
            endif()
            set(pragma_path "${generated_dir}/${overlay}_sections.h")
            file(CONFIGURE OUTPUT "${pragma_path}" CONTENT [=[
/* Written by port/cmake/OverlaySections.cmake; don't edit */
#pragma clang section data="__DATA,@data_section@" bss="__DATA,@bss_section@,zerofill"
]=] @ONLY)
            target_compile_options(${prefix} PRIVATE "SHELL:-include \"${pragma_path}\"")
            string(APPEND declarations
                "extern unsigned char ${prefix}_data_start[] "
                "__asm(\"section$start$__DATA$${data_section}\");\n"
                "extern unsigned char ${prefix}_data_end[] "
                "__asm(\"section$end$__DATA$${data_section}\");\n"
                "extern unsigned char ${prefix}_bss_start[] "
                "__asm(\"section$start$__DATA$${bss_section}\");\n"
                "extern unsigned char ${prefix}_bss_end[] "
                "__asm(\"section$end$__DATA$${bss_section}\");\n")
        else()
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
        endif()
        string(APPEND entries
            "    {${overlay}_SEGMENT_NUMBER, ${prefix}_data_start, ${prefix}_data_end, "
            "${prefix}_bss_start, ${prefix}_bss_end},\n")
    endforeach()

    if(NOT APPLE)
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
        target_link_options(${arg_TARGET} PRIVATE "LINKER:-T,${script_path}")
        set_property(TARGET ${arg_TARGET} APPEND PROPERTY LINK_DEPENDS "${script_path}")
    endif()

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
endfunction()
