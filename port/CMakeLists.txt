# The port's targets: the game's code compiled for the host, our own
# libultra, NuSystem and libmus, the renderer, and the program that joins them.

# The switches that give what C leaves undefined KMC GCC's meaning. A
# sanitizer build turns them off, so that its checks see the undefined
# behavior they would hide, and the port can drop each once none is left.
# The sanitizers check it, so it comes first.
option(MG64_KMC_SEMANTICS "Give the game's undefined behavior KMC GCC's meaning" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# Before any target, SDL's included, for the sanitizers to cover them all
include(Sanitizers)
include(GameSources)
include(Dependencies)

# Every library that the port's targets link is a target from here on, so
# that a misspelt name stops the configure rather than reaching the linker,
# which could find a library of that name. SDL's targets, added above, link
# system libraries by name.
set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)

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
        "-DTEXTURES=${PROJECT_SOURCE_DIR}/textures.txt"
        "-DOUTPUT=${MG64_GENERATED_DIR}/rom_file_offsets.h"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateRomFiles.cmake"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/assets.txt"
        "${PROJECT_SOURCE_DIR}/undefined_syms.txt"
        "${PROJECT_SOURCE_DIR}/layout.txt"
        "${PROJECT_SOURCE_DIR}/textures.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateRomFiles.cmake"
    COMMENT "Writing the ROM file offsets"
    VERBATIM
)

include(TextureArrays)
mg64_write_texture_array_sizes(
    LIST "${PROJECT_SOURCE_DIR}/textures.txt"
    OUTPUT_DIR "${MG64_GENERATED_DIR}"
)

# What every target that includes the game's or libultra's headers needs.
# The port's headers come first, to stand in for libultra's where they
# differ. The generated headers are its sources, which CMake writes before
# it compiles any target that links it.
add_library(mg64_headers INTERFACE
    "${MG64_GENERATED_DIR}/PR/gbi.h"
    "${MG64_GENERATED_DIR}/rom_file_offsets.h"
)
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
target_link_libraries(mg64_headers INTERFACE mg64_project_sanitizers)
# The game's code was written for KMC GCC, whose char is unsigned, and which
# took it as it reads where modern GCC would assume that undefined behavior
# can't happen (see the Makefile's KMC_SEMANTICS). Contracting multiplies and
# adds into fused ones would change the ball's flight from one host to
# another, and with it the demos' and replays' outcomes. Clang has no switch
# to match GCC's against loop bounds taken from undefined behavior: -fwrapv
# covers its overflows, and AVOID_UB the game's known reads past arrays.
target_compile_options(mg64_headers INTERFACE
    -funsigned-char
    -fno-common
    -ffp-contract=off
)
# KMC GCC's meanings, which MG64_KMC_SEMANTICS chooses above. The game's
# accesses of memory as other types than its own are fixed (see AVOID_UB),
# as TypeSanitizer finds none left in the playthroughs, so the port no
# longer needs -fno-strict-aliasing, which KMC GCC's code was built with.
if(MG64_KMC_SEMANTICS)
    target_compile_options(mg64_headers INTERFACE
        -fwrapv
        $<$<C_COMPILER_ID:GNU>:-fno-aggressive-loop-optimizations>
        -fno-delete-null-pointer-checks
    )
endif()

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

if(ANDROID)
    add_subdirectory(android)
endif()

# A cross build runs its tests through CMAKE_CROSSCOMPILING_EMULATOR, such
# as Wine for Windows. Without one, CTest could only fail to start each test.
if(BUILD_TESTING AND CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    message(WARNING
        "Nothing here runs ${CMAKE_SYSTEM_NAME} programs, so the tests are left out. "
        "To run them, install Wine and configure again."
    )
elseif(BUILD_TESTING)
    add_subdirectory(tests)
endif()
