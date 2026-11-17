# Writes the port's gbi.h from libultra's.
#
# A display list command on the N64 is two 32-bit words, the second of which
# often holds an address. A 64-bit host's addresses don't fit in 32 bits, so
# the port's commands hold two pointer-sized words instead, and the macros
# that store an address cast it to uintptr_t rather than to unsigned int.
# Mtx_t's elements are long, which is 64 bits wide on LP64 hosts; the RSP
# reads them as 32-bit words. The colour and depth packing macros give
# unsigned values, as the game shifts them into a fill color's high half,
# which a signed int's sign bit makes undefined.
#
# Run as a script: cmake -DINPUT=<libultra gbi.h> -DOUTPUT=<gbi.h> -P GenerateGbi.cmake

cmake_minimum_required(VERSION 3.30)

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "GenerateGbi.cmake needs INPUT and OUTPUT")
endif()

file(READ "${INPUT}" gbi)

# Each replacement must change something, or libultra's header has changed
# under the port and the result can't be trusted
function(replace_required pattern replacement)
    string(REGEX REPLACE "${pattern}" "${replacement}" replaced "${gbi}")
    if(replaced STREQUAL gbi)
        message(FATAL_ERROR "GenerateGbi: no match for \"${pattern}\" in ${INPUT}")
    endif()
    set(gbi "${replaced}" PARENT_SCOPE)
endfunction()

replace_required("typedef long([ \t]+)Mtx_t" "typedef int\\1Mtx_t")
replace_required(
    "typedef struct {[ \t\r\n]+unsigned int w0;[ \t\r\n]+unsigned int w1;[ \t\r\n]+} Gwords;"
    "typedef struct {\n\tuintptr_t w0;\n\tuintptr_t w1;\n} Gwords;"
)
replace_required("\\(unsigned int\\)[ \t]*\\(" "(uintptr_t)(")
replace_required(
    "(#define[ \t]+GPACK_RGBA5551\\(r, g, b, a\\)[ \t]+)(\\(\\(\\(\\(r\\)<<8\\))"
    "\\1((unsigned int)\\2"
)
replace_required("(\\(\\(a\\) & 0x1\\)\\))" "\\1)")
replace_required(
    "(#define[ \t]+GPACK_ZDZ\\(z, dz\\)[ \t]+)(\\(\\(z\\) << 2 \\| \\(dz\\)\\))"
    "\\1((unsigned int)\\2)"
)
replace_required("#define[ \t]+_GBI_H_" "#define _GBI_H_\n\n#include <stdint.h>")

set(banner "/* Written by port/cmake/GenerateGbi.cmake from ${INPUT}; don't edit */\n")
file(WRITE "${OUTPUT}" "${banner}${gbi}")
