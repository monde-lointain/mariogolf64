# Reads the game's C sources from layout.txt, the list that the N64 build
# links, so that a file added to the game is built here too.
#
# Sets MG64_GAME_SOURCES to every C file of every segment but those marked
# "matching", which only the matching ROM links, and MG64_OVERLAY_<NAME>_SOURCES
# to the files of each overlay segment, whose variables are reset as the
# overlay loads. MG64_OVERLAY_NAMES lists the overlays.

set(layout_file "${PROJECT_SOURCE_DIR}/layout.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${layout_file}")

file(STRINGS "${layout_file}" layout_lines)

set(MG64_GAME_SOURCES "")
set(MG64_OVERLAY_NAMES "")
set(segment "")
foreach(line IN LISTS layout_lines)
    string(REGEX REPLACE "#.*" "" line "${line}")
    if(line MATCHES "^segment +([A-Za-z_0-9]+)")
        set(segment "${CMAKE_MATCH_1}")
        if(segment MATCHES "^ovl_")
            list(APPEND MG64_OVERLAY_NAMES "${segment}")
            set(MG64_OVERLAY_${segment}_SOURCES "")
        endif()
        continue()
    endif()
    if(line MATCHES "^file ")
        set(segment "")
        continue()
    endif()
    if(NOT line MATCHES "^[ \t]+(src/[^ \t]+\\.c)(.*)$")
        continue()
    endif()
    set(source "${CMAKE_MATCH_1}")
    set(kinds "${CMAKE_MATCH_2}")
    if(kinds MATCHES "matching")
        continue()
    endif()
    set(path "${PROJECT_SOURCE_DIR}/${source}")
    if(NOT path IN_LIST MG64_GAME_SOURCES)
        list(APPEND MG64_GAME_SOURCES "${path}")
        if(segment MATCHES "^ovl_")
            list(APPEND MG64_OVERLAY_${segment}_SOURCES "${path}")
        endif()
    endif()
endforeach()

if(NOT MG64_GAME_SOURCES)
    message(FATAL_ERROR "No C sources found in ${layout_file}")
endif()
