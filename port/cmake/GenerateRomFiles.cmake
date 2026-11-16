# Writes rom_file_offsets.h: the names that the N64 build's linker script
# defines at the bounds of the ROM's files and segments, as constants.
#
# The N64 build's linker defines NAME_ROM_START and NAME_ROM_END for each file
# that layout.txt places, named after its path below assets/. The port reads
# those files from the original ROM, so it takes their offsets in that ROM
# from assets.txt, and the names that the game's code gives some of them from
# undefined_syms.txt.
#
# The code segments are linked into the program, so their symbols only need
# to tell the segments apart: a segment's ROM_START and ROM_END are its
# number, counting from 1 in layout.txt's order, and its VRAM, TEXT_END,
# BSS_START and BSS_END all the address that it runs at on the N64, in
# osPcRdram, the port's copy of the N64's memory (see PR/os_pc.h). The
# overlay manager takes overlays that run at the same address to exclude one
# another, as they did on the N64, where those that run at different
# addresses never overlap.
#
# Run as a script:
#   cmake -DASSETS=<assets.txt> -DSYMBOLS=<undefined_syms.txt> -DLAYOUT=<layout.txt>
#         -DOUTPUT=<rom_file_offsets.h> -P GenerateRomFiles.cmake

cmake_minimum_required(VERSION 3.30)

foreach(variable IN ITEMS ASSETS SYMBOLS LAYOUT OUTPUT)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "GenerateRomFiles.cmake needs ${variable}")
    endif()
endforeach()

# The build date is a segment of the game's own, so assets.txt doesn't list
# it; it lies just before the sound pointer bank
set(BUILD_DATE_ROM_START 0x0171770)
set(BUILD_DATE_ROM_END 0x0171790)

set(header [=[
/* Written by port/cmake/GenerateRomFiles.cmake; don't edit */

#ifndef ROM_FILE_OFFSETS_H
#define ROM_FILE_OFFSETS_H

#include <stdint.h>

/* An offset in the ROM, as the address that the N64 build's symbol has */
#define RomFileAddress(offset) ((unsigned char *)(uintptr_t)(offset))

]=])

string(APPEND header
    "#define build_date_ROM_START RomFileAddress(${BUILD_DATE_ROM_START})\n"
    "#define build_date_ROM_END RomFileAddress(${BUILD_DATE_ROM_END})\n")

file(STRINGS "${ASSETS}" asset_lines REGEX "^0x[0-9A-Fa-f]+ +0x[0-9A-Fa-f]+ +assets/")
foreach(line IN LISTS asset_lines)
    if(NOT line MATCHES "^(0x[0-9A-Fa-f]+) +(0x[0-9A-Fa-f]+) +assets/([^ ]+)\\.bin")
        continue()
    endif()
    set(start "${CMAKE_MATCH_1}")
    set(end "${CMAKE_MATCH_2}")
    string(REPLACE "/" "_" name "${CMAKE_MATCH_3}")
    string(APPEND header
        "#define ${name}_ROM_START RomFileAddress(${start})\n"
        "#define ${name}_ROM_END RomFileAddress(${end})\n")
endforeach()

string(APPEND header "\n/* The names that the game's code gives some of the files */\n")
file(STRINGS "${SYMBOLS}" symbol_lines REGEX "^[A-Za-z_0-9]+ = [A-Za-z_0-9]+_ROM_(START|END);")
foreach(line IN LISTS symbol_lines)
    string(REGEX MATCH "^([A-Za-z_0-9]+) = ([A-Za-z_0-9]+);" match "${line}")
    string(APPEND header "#define ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\n")
endforeach()

string(APPEND header "\n/* The code segments: each one's number, and its address on the N64 */\n")
file(STRINGS "${LAYOUT}" segment_lines REGEX "^segment +[A-Za-z_0-9]+ +0x[0-9A-Fa-f]+")
set(segment_number 0)
foreach(line IN LISTS segment_lines)
    string(REGEX MATCH "^segment +([A-Za-z_0-9]+) +(0x[0-9A-Fa-f]+)" match "${line}")
    set(name "${CMAKE_MATCH_1}")
    set(address "${CMAKE_MATCH_2}")
    math(EXPR segment_number "${segment_number} + 1")
    string(APPEND header "#define ${name}_SEGMENT_NUMBER ${segment_number}\n")
    if(name STREQUAL "build_date")
        continue()
    endif()
    string(APPEND header
        "#define ${name}_ROM_START RomFileAddress(${segment_number})\n"
        "#define ${name}_ROM_END RomFileAddress(${segment_number})\n")
    foreach(bound IN ITEMS VRAM TEXT_END BSS_START BSS_END)
        string(APPEND header "#define ${name}_${bound} PcRdramAddress(${address})\n")
    endforeach()
endforeach()
string(APPEND header "#define SEGMENT_COUNT ${segment_number}\n")

string(APPEND header "\n#endif\n")
file(WRITE "${OUTPUT}" "${header}")
