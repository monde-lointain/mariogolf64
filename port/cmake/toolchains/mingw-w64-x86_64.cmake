# Cross-compiles the port for 64-bit Windows with MinGW-w64's GCC, as the
# windows-mingw preset does. The build's own tools (Python, glslang and
# spirv-cross) run on the host, so programs are found there, and libraries
# and headers only in MinGW's root.
#
# CMAKE_CROSSCOMPILING_EMULATOR lets CTest run the tests through Wine, where
# it is installed.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(MG64_MINGW_PREFIX x86_64-w64-mingw32 CACHE STRING "Prefix of MinGW-w64's tools")

set(CMAKE_C_COMPILER ${MG64_MINGW_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${MG64_MINGW_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${MG64_MINGW_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${MG64_MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

find_program(MG64_WINE NAMES wine wine64)
if(MG64_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${MG64_WINE}")
endif()
