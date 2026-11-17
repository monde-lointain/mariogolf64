# Builds everything that follows, SDL included, with the sanitizers that
# MG64_SANITIZERS lists, such as "address;undefined". Their runtime options
# come from the environment (ASAN_OPTIONS and the like), which the test
# presets set.
#
# TypeSanitizer is the exception: it checks only the project's own code,
# through mg64_project_sanitizers, which the project's targets link. SDL's
# code reads its objects under other types than it stores them, and the
# runtime crashes on what SDL's vector code leaves in its shadow. Each of
# the project's programs also forgets the types it gives static storage
# (see forget_static_types.c), and so needs Linux's names for its ends.
#
# TypeSanitizer also leaves out the code that mg64_skip_type_sanitizer
# names: the renderer and the synthesizer. They read the game's memory as
# bytes, with memcpy, or (for display lists) as the Gfx the game wrote,
# and each byte read of memory that the game stored under its own types
# takes the runtime's slow path. That made the replays eight times
# slower. Their own structures go unchecked as a result.
#
# Only a top-level build takes them, so that a project that includes this
# one keeps its own choice.

add_library(mg64_project_sanitizers INTERFACE)

# Leaves the target's own sources out of TypeSanitizer's checks
function(mg64_skip_type_sanitizer target)
    if(PROJECT_IS_TOP_LEVEL AND "type" IN_LIST MG64_SANITIZERS)
        get_target_property(sources ${target} SOURCES)
        set_property(SOURCE ${sources} TARGET_DIRECTORY ${target}
            APPEND PROPERTY COMPILE_OPTIONS -fno-sanitize=type
        )
    endif()
endfunction()

set(MG64_SANITIZERS "" CACHE STRING
    "The sanitizers to build with, as -fsanitize names in a list, such as address;undefined"
)

if(NOT PROJECT_IS_TOP_LEVEL OR NOT MG64_SANITIZERS)
    return()
endif()

if(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
    message(FATAL_ERROR "MG64_SANITIZERS needs GCC or Clang, not ${CMAKE_C_COMPILER_ID}")
endif()

# Each of these keeps its own shadow of memory, so no two can share a program
set(exclusive_sanitizers "")
foreach(sanitizer IN ITEMS address thread memory type)
    if(sanitizer IN_LIST MG64_SANITIZERS)
        list(APPEND exclusive_sanitizers ${sanitizer})
    endif()
endforeach()
list(LENGTH exclusive_sanitizers exclusive_count)
if(exclusive_count GREATER 1)
    message(FATAL_ERROR
        "MG64_SANITIZERS can hold only one of address, thread, memory and type, not "
        "${exclusive_sanitizers}"
    )
endif()

# TypeSanitizer is Clang's alone. It checks the accesses that strict aliasing
# lets the optimizer assume apart.
if("type" IN_LIST MG64_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_VERSION VERSION_LESS 23)
        message(FATAL_ERROR
            "MG64_SANITIZERS' type needs Clang 23 or later, whose runtime can "
            "forget types, not ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
        )
    endif()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "MG64_SANITIZERS' type needs Linux")
    endif()
endif()

# Clang's checks of pointer pairs also take a difference or comparison of
# addresses, converted to integers, for one of pointers, which C defines.
# GCC's take only those of pointers.
foreach(sanitizer IN ITEMS pointer-compare pointer-subtract)
    if(sanitizer IN_LIST MG64_SANITIZERS)
        if(NOT "address" IN_LIST MG64_SANITIZERS)
            message(FATAL_ERROR "MG64_SANITIZERS' ${sanitizer} needs address with it")
        endif()
        if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
            message(FATAL_ERROR
                "MG64_SANITIZERS' ${sanitizer} needs GCC: Clang's also stops at "
                "differences and comparisons of addresses as integers"
            )
        endif()
    endif()
endforeach()

set(program_sanitizers ${MG64_SANITIZERS})
list(REMOVE_ITEM program_sanitizers type)
if(program_sanitizers)
    list(JOIN program_sanitizers "," sanitizer_list)
    add_compile_options(-fsanitize=${sanitizer_list} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${sanitizer_list})
endif()
if("type" IN_LIST MG64_SANITIZERS)
    target_compile_options(mg64_project_sanitizers INTERFACE
        -fsanitize=type
        -fno-omit-frame-pointer
    )
    target_link_options(mg64_project_sanitizers INTERFACE -fsanitize=type)
    target_sources(mg64_project_sanitizers INTERFACE
        "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${CMAKE_CURRENT_LIST_DIR}/../src/sanitizers/forget_static_types.c>"
    )
endif()
