# Builds everything that follows, SDL included, with the sanitizers that
# MG64_SANITIZERS lists, such as "address;undefined". Their runtime options
# come from the environment (ASAN_OPTIONS and the like), which the test
# presets set.
#
# Only a top-level build takes them, so that a project that includes this
# one keeps its own choice.

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
foreach(sanitizer IN ITEMS address thread memory)
    if(sanitizer IN_LIST MG64_SANITIZERS)
        list(APPEND exclusive_sanitizers ${sanitizer})
    endif()
endforeach()
list(LENGTH exclusive_sanitizers exclusive_count)
if(exclusive_count GREATER 1)
    message(FATAL_ERROR
        "MG64_SANITIZERS can hold only one of address, thread and memory, not "
        "${exclusive_sanitizers}"
    )
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

list(JOIN MG64_SANITIZERS "," sanitizer_list)
add_compile_options(-fsanitize=${sanitizer_list} -fno-omit-frame-pointer)
add_link_options(-fsanitize=${sanitizer_list})
