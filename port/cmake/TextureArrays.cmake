# The textures that the game's code holds in C arrays, which a native build
# reads from the player's ROM at start-up (see include/system/rom_arrays.h)
# rather than holding a copy. The texels that the N64 build includes in
# each array's initializer, which tools/extract_textures.py cuts from the
# ROM, are zeros here, as many as the N64 build's, so that the array has
# the same size.

# Writes, for each texture that LIST names, OUTPUT_DIR/<its PNG's path less
# .png>.inc.c, which sizes its array. The configure runs again when LIST
# changes.
function(mg64_write_texture_array_sizes)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "LIST;OUTPUT_DIR" "")
    if(NOT arg_LIST OR NOT arg_OUTPUT_DIR OR arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "mg64_write_texture_array_sizes needs LIST and OUTPUT_DIR alone")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${arg_LIST}")
    file(STRINGS "${arg_LIST}" lines)
    # Bits of each texel, and bytes of each array element, by format
    set(texel_bits_i4 4)
    set(texel_bits_ia4 4)
    set(texel_bits_i8 8)
    set(texel_bits_rgba16 16)
    set(element_size_i4 1)
    set(element_size_ia4 2)
    set(element_size_i8 1)
    set(element_size_rgba16 2)
    foreach(line IN LISTS lines)
        string(REGEX REPLACE "#.*" "" line "${line}")
        if(NOT line MATCHES "^0x[0-9A-Fa-f]+ +([a-z0-9]+) +([0-9]+) +([0-9]+) +(assets/[^ \t]+)\\.png")
            continue()
        endif()
        set(format "${CMAKE_MATCH_1}")
        if(NOT DEFINED texel_bits_${format})
            message(FATAL_ERROR "textures.txt gives ${CMAKE_MATCH_4} the unknown format ${format}")
        endif()
        math(EXPR last_element
            "${CMAKE_MATCH_2} * ${CMAKE_MATCH_3} * ${texel_bits_${format}} / 8 / ${element_size_${format}} - 1")
        file(CONFIGURE
            OUTPUT "${arg_OUTPUT_DIR}/${CMAKE_MATCH_4}.inc.c"
            CONTENT "/* Written by port/cmake/TextureArrays.cmake from textures.txt; don't edit */\n[${last_element}] = 0\n"
        )
    endforeach()
endfunction()
