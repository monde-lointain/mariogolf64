# The game's strings are Shift-JIS, as the N64 build compiles them, but its
# sources are UTF-8. Rather than ask each compiler for an execution character
# set, which Clang and MSVC can't give as Shift-JIS, the build compiles a
# Shift-JIS copy of each source, as the N64's Makefile does. The copy also has
# the game's nested functions at file scope, for compilers without them.

# mg64_encode_shift_jis(<out-var> <source>...)
#
# Adds a custom command for each source that writes its copy under the
# current binary directory, and sets <out-var> to the copies in the same
# order.
function(mg64_encode_shift_jis out_var)
    set(script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/encode_shift_jis.py")
    set(hoister "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/nested_functions.py")
    set(copies "")
    foreach(source IN LISTS ARGN)
        cmake_path(RELATIVE_PATH source BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
            OUTPUT_VARIABLE relative)
        set(copy "${CMAKE_CURRENT_BINARY_DIR}/shift_jis/${relative}")
        add_custom_command(
            OUTPUT "${copy}"
            COMMAND Python3::Interpreter "${script}" "${source}" "${copy}"
            DEPENDS "${source}" "${script}" "${hoister}"
            COMMENT "Encoding ${relative} as Shift-JIS"
            VERBATIM
        )
        list(APPEND copies "${copy}")
    endforeach()
    set(${out_var} "${copies}" PARENT_SCOPE)
endfunction()
