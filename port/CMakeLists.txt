# The port's targets: the game's code compiled for the host, our own
# libultra, NuSystem and libmus, the renderer, and the program that joins them.

# The game's code is C89 in GNU C's dialect: C23 would take its empty
# parameter lists to mean no parameters
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# Before any target, SDL's included, for the sanitizers to cover them all
include(Sanitizers)
include(GameSources)
include(Dependencies)

set(MG64_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_custom_command(
    OUTPUT "${MG64_GENERATED_DIR}/PR/gbi.h"
    COMMAND "${CMAKE_COMMAND}"
        "-DINPUT=${PROJECT_SOURCE_DIR}/lib/ultralib/include/PR/gbi.h"
        "-DOUTPUT=${MG64_GENERATED_DIR}/PR/gbi.h"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateGbi.cmake"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/lib/ultralib/include/PR/gbi.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateGbi.cmake"
    COMMENT "Writing the port's gbi.h"
    VERBATIM
)

add_custom_command(
    OUTPUT "${MG64_GENERATED_DIR}/rom_file_offsets.h"
    COMMAND "${CMAKE_COMMAND}"
        "-DASSETS=${PROJECT_SOURCE_DIR}/assets.txt"
        "-DSYMBOLS=${PROJECT_SOURCE_DIR}/undefined_syms.txt"
        "-DLAYOUT=${PROJECT_SOURCE_DIR}/layout.txt"
        "-DOUTPUT=${MG64_GENERATED_DIR}/rom_file_offsets.h"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateRomFiles.cmake"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/assets.txt"
        "${PROJECT_SOURCE_DIR}/undefined_syms.txt"
        "${PROJECT_SOURCE_DIR}/layout.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateRomFiles.cmake"
    COMMENT "Writing the ROM file offsets"
    VERBATIM
)

# The textures that the game's code holds in C arrays, from the PNGs that
# tools/extract_textures.py writes from the ROM
set(texture_list "${PROJECT_SOURCE_DIR}/textures.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${texture_list}")
file(STRINGS "${texture_list}" texture_lines)
set(texture_includes "")
foreach(line IN LISTS texture_lines)
    string(REGEX REPLACE "#.*" "" line "${line}")
    if(NOT line MATCHES "(assets/[^ \t]+)\\.png")
        continue()
    endif()
    set(png "${PROJECT_SOURCE_DIR}/${CMAKE_MATCH_1}.png")
    set(texels "${MG64_GENERATED_DIR}/${CMAKE_MATCH_1}.inc.c")
    add_custom_command(
        OUTPUT "${texels}"
        COMMAND Python3::Interpreter
            "${PROJECT_SOURCE_DIR}/tools/png_to_c.py" "${png}" "${texels}"
        DEPENDS "${png}" "${PROJECT_SOURCE_DIR}/tools/png_to_c.py"
        COMMENT "Converting ${CMAKE_MATCH_1}.png"
        VERBATIM
    )
    list(APPEND texture_includes "${texels}")
endforeach()

add_custom_target(mg64_generated_headers
    DEPENDS
        "${MG64_GENERATED_DIR}/PR/gbi.h"
        "${MG64_GENERATED_DIR}/rom_file_offsets.h"
        ${texture_includes}
)

# What every target that includes the game's or libultra's headers needs.
# The port's headers come first, to stand in for libultra's where they
# differ.
add_library(mg64_headers INTERFACE)
add_dependencies(mg64_headers mg64_generated_headers)
target_include_directories(mg64_headers INTERFACE
    "${MG64_GENERATED_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/lib/ultralib/include"
    "${PROJECT_SOURCE_DIR}/lib/libmus/include"
    "${PROJECT_SOURCE_DIR}/lib/libnusys/include"
    "${PROJECT_SOURCE_DIR}/src"
)
target_compile_definitions(mg64_headers INTERFACE
    TARGET_PC
    _LANGUAGE_C
    F3DEX_GBI_2
    NDEBUG
    _FINALROM
    BUILD_VERSION=VERSION_J
    NON_MATCHING
    AVOID_UB
    # Declares libultra's bzero, bcopy and bcmp as the host's C library does
    MODERN_CC
)
target_compile_features(mg64_headers INTERFACE c_std_11)
# The game's code was written for KMC GCC, whose char is unsigned, and which
# took it as it reads where modern GCC would assume that undefined behavior
# can't happen (see the Makefile's KMC_SEMANTICS). Contracting multiplies and
# adds into fused ones would change the ball's flight from one host to
# another, and with it the demos' and replays' outcomes. Clang has no switch
# to match GCC's against loop bounds taken from undefined behavior: -fwrapv
# covers its overflows, and AVOID_UB the game's known reads past arrays.
target_compile_options(mg64_headers INTERFACE
    -funsigned-char
    -fno-strict-aliasing
    -fwrapv
    $<$<C_COMPILER_ID:GNU>:-fno-aggressive-loop-optimizations>
    -fno-delete-null-pointer-checks
    -fno-common
    -ffp-contract=off
)

# The headers that the port's own libraries share among themselves
add_library(mg64_port_internal INTERFACE)
target_include_directories(mg64_port_internal INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/include")
target_link_libraries(mg64_port_internal INTERFACE mg64_headers)

# The warnings that the port's own code is held to
add_library(mg64_port_warnings INTERFACE)
target_compile_options(mg64_port_warnings INTERFACE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
