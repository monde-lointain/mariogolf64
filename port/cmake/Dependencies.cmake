# Finds what the port builds against: SDL 3 for the window, input, audio and
# GPU, the C library's maths functions, and the Python that runs the build's
# scripts.
#
# SDL is downloaded and built as part of the project, linked statically, so
# that every build has the same SDL and needs nothing installed to run. The
# download is SDL's release archive, checked against its hash, which a tag
# in SDL's repository couldn't guarantee: a tag can move. The archive also
# holds the android-project that the Android app is packed from.
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
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz
        URL_HASH SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
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

# The C library's maths functions, which Unix C libraries keep apart in
# libm. Windows' C runtime holds them itself; MinGW's libm is an empty
# stand-in.
add_library(mg64_math INTERFACE IMPORTED)
if(NOT WIN32)
    set_target_properties(mg64_math PROPERTIES IMPORTED_LIBNAME m)
endif()

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
