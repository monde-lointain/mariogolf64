# The native PC port. The N64 ROM is built by the Makefile; this builds the
# same game code as a program for the host (see port/README.md).

cmake_minimum_required(VERSION 3.30)

project(mariogolf64
    VERSION 1.0.0
    DESCRIPTION "Native port of the Mario Golf decompilation"
    LANGUAGES C
)

# The game's code is C89 in GNU C's dialect, which it needs exactly: C23
# would take its empty parameter lists to mean no parameters, and is the
# default of GCC 15 and later. So the standard is set, not only asked for
# as a minimum, which a compiler's newer default would satisfy.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# SDL's Android code has some C++, whose runtime the program then links
if(ANDROID)
    enable_language(CXX)
endif()

# The renderer tells the host's addresses from the N64's by whether they
# need more than 32 bits. A position-independent executable loads above
# 4 GiB on every 64-bit host, but a GCC that links programs at a fixed
# address by default, such as Fedora's, would put their data below it.
include(CheckPIESupported)
check_pie_supported()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

add_subdirectory(port)
