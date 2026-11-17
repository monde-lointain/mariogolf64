# Finds what the port builds against: SDL 3 for the window, input, audio and
# GPU, and the Python that converts the texture PNGs.
#
# SDL is taken from an installed package when there is one, and otherwise
# downloaded and built as part of the project, linked statically, so that the
# Windows and Android builds need nothing installed.

include(FetchContent)

# 3.4 is the first with SDL_SavePNG, which writes --screenshot's image
set(MG64_SDL_MINIMUM_VERSION 3.4.0)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.20
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS ${MG64_SDL_MINIMUM_VERSION} CONFIG
)
set(SDL_SHARED OFF CACHE BOOL "")
set(SDL_STATIC ON CACHE BOOL "")
set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
FetchContent_MakeAvailable(SDL3)

if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "SDL3 provided no SDL3::SDL3 target")
endif()

# The project's own virtual environment has Pillow, which png_to_c.py needs
if(NOT DEFINED Python3_EXECUTABLE AND EXISTS "${PROJECT_SOURCE_DIR}/.venv/bin/python3")
    set(Python3_EXECUTABLE "${PROJECT_SOURCE_DIR}/.venv/bin/python3")
endif()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
