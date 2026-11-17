# Finds what the port builds against: SDL 3 for the window, input, audio and
# GPU, and the Python that converts the texture PNGs.
#
# SDL is downloaded and built as part of the project, linked statically, so
# that every build has the same SDL and needs nothing installed to run.
# Distributions' SDL can also lack libusb, through which alone SDL reaches
# the Wii U GameCube controller adapter (Ubuntu's does). MG64_USE_SYSTEM_SDL
# builds against an installed SDL instead.

include(FetchContent)

# 3.4 is the first with SDL_SavePNG, which writes --screenshot's image
set(MG64_SDL_MINIMUM_VERSION 3.4.0)

option(MG64_USE_SYSTEM_SDL "Build against an installed SDL 3 instead of building one" OFF)

if(MG64_USE_SYSTEM_SDL)
    find_package(SDL3 ${MG64_SDL_MINIMUM_VERSION} REQUIRED CONFIG)
else()
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.20
        GIT_SHALLOW TRUE
    )
    set(SDL_SHARED OFF CACHE BOOL "")
    set(SDL_STATIC ON CACHE BOOL "")
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "")
    # SDL uses libusb where its headers are found, and loads the library when
    # it starts, so the program still runs without it, minus the adapter
    set(SDL_HIDAPI_LIBUSB ON CACHE BOOL "")
    FetchContent_MakeAvailable(SDL3)
endif()

if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "SDL3 provided no SDL3::SDL3 target")
endif()

# The project's own virtual environment has Pillow, which png_to_c.py needs
if(NOT DEFINED Python3_EXECUTABLE AND EXISTS "${PROJECT_SOURCE_DIR}/.venv/bin/python3")
    set(Python3_EXECUTABLE "${PROJECT_SOURCE_DIR}/.venv/bin/python3")
endif()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
