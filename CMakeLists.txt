# The native PC port. The N64 ROM is built by the Makefile; this builds the
# same game code as a program for the host (see port/README.md).

cmake_minimum_required(VERSION 3.30)

project(mariogolf64
    VERSION 1.0.0
    DESCRIPTION "Native port of the Mario Golf decompilation"
    LANGUAGES C
)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

add_subdirectory(port)
