# Writes the port's version.c: the project's version, and the git revision
# that the program was built from, with "-dirty" when the tree had changes,
# or "unknown" outside a git checkout. It runs at each build, as the
# revision changes without CMake knowing, and rewrites the file only when
# the text changes, so that an unchanged revision rebuilds nothing.
#
# Run as a script: cmake -DVERSION=<x.y.z> -DSOURCE_DIR=<repository>
#     -DOUTPUT=<version.c> -P WriteVersion.cmake

cmake_minimum_required(VERSION 3.30)

if(NOT DEFINED VERSION OR NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "WriteVersion.cmake needs VERSION, SOURCE_DIR and OUTPUT")
endif()

set(revision "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --abbrev=8 --exclude=*
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE described
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE result
        ERROR_QUIET
    )
    if(result EQUAL 0 AND NOT described STREQUAL "")
        set(revision "${described}")
    endif()
endif()

set(text "// Written by WriteVersion.cmake at each build\n\n#include \"version.h\"\n\nconst char g_PortVersion[] = \"${VERSION}\";\nconst char g_PortRevision[] = \"${revision}\";\n")
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" old)
endif()
if(NOT old STREQUAL text)
    file(WRITE "${OUTPUT}" "${text}")
endif()
