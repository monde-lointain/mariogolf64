set(MG64_WORK "${PROJECT_BINARY_DIR}/work")
set(MG64_HELPER "${PROJECT_SOURCE_DIR}/tools/rom_build.py")
set(MG64_RUN "${CMAKE_COMMAND}" -E env "MG64_BUILD_DIR=${PROJECT_BINARY_DIR}" "${MG64_PYTHON}")
set(MG64_BASE_ROM "${PROJECT_SOURCE_DIR}/baserom.z64" CACHE FILEPATH "Private US retail NMFE revision 0 ROM")
option(MG64_NONMATCHING "Compile nonmatching C; never eligible for ROM verification" OFF)

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "UV_PROJECT_ENVIRONMENT=${PROJECT_SOURCE_DIR}/.venv"
    "${MG64_UV}" run --locked --project "${PROJECT_SOURCE_DIR}" python "${PROJECT_SOURCE_DIR}/tools/rom_manifest.py"
    --output "${PROJECT_BINARY_DIR}/rom-sources.json"
    COMMAND_ERROR_IS_FATAL ANY)
file(READ "${PROJECT_BINARY_DIR}/rom-sources.json" manifest)
string(JSON source_count LENGTH "${manifest}" sources)
math(EXPR last_source "${source_count} - 1")
set(original_sources)
set(extracted_sources)
set(object_files)
foreach(index RANGE ${last_source})
    string(JSON relative GET "${manifest}" sources ${index} path)
    string(JSON kind GET "${manifest}" sources ${index} kind)
    if(kind MATCHES "^(c|hasm)$")
        set(source "${PROJECT_SOURCE_DIR}/${relative}")
        list(APPEND original_sources "${source}")
    else()
        set(source "${MG64_WORK}/${relative}")
        list(APPEND extracted_sources "${source}")
    endif()
    string(REGEX REPLACE "\\.[^.]+$" ".o" object_relative "${relative}")
    set(object "${PROJECT_BINARY_DIR}/obj/${object_relative}")
    get_filename_component(object_directory "${object}" DIRECTORY)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" object_name "${object_relative}")
    if(kind STREQUAL "c")
        set(extra_flags)
        if(MG64_NONMATCHING)
            list(APPEND extra_flags --nonmatching)
        endif()
        add_custom_command(OUTPUT "${object}"
            COMMAND ${MG64_RUN} "${MG64_HELPER}" compile --source "${source}" --output "${object}"
                --depfile "${object}.d" ${extra_flags}
            DEPENDS "${source}" "${MG64_HELPER}" "${PROJECT_SOURCE_DIR}/tools/build_profiles.json"
                "${PROJECT_BINARY_DIR}/build-tools.json" mg64_tool_gcc mg64_tool_kmc_as mg64_tool_strip extract
            DEPFILE "${object}.d" VERBATIM)
    elseif(kind STREQUAL "bin")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${object_directory}"
            COMMAND mg64_tool_objcopy -I binary -O elf32-tradbigmips "${relative}" "${object}.tmp"
            COMMAND "${CMAKE_COMMAND}" -E rename "${object}.tmp" "${object}"
            DEPENDS "${source}" mg64_tool_objcopy extract
            WORKING_DIRECTORY "${MG64_WORK}" VERBATIM)
    else()
        add_custom_command(OUTPUT "${object}"
            COMMAND ${MG64_RUN} "${MG64_HELPER}" assemble --source "${source}" --output "${object}" --depfile "${object}.d"
            DEPENDS "${source}" "${MG64_HELPER}" "${PROJECT_BINARY_DIR}/build-tools.json"
                mg64_tool_gcc mg64_tool_kmc_as mg64_tool_as mg64_tool_objcopy extract
            DEPFILE "${object}.d" VERBATIM)
    endif()
    add_custom_target(mg64_obj_${object_name} DEPENDS "${object}")
    if(NOT kind STREQUAL "reference")
        list(APPEND object_files "${object}")
    endif()
endforeach()

string(JSON byproduct_count LENGTH "${manifest}" byproducts)
if(byproduct_count GREATER 0)
    math(EXPR last_byproduct "${byproduct_count} - 1")
    foreach(index RANGE ${last_byproduct})
        string(JSON relative GET "${manifest}" byproducts ${index})
        list(APPEND extracted_sources "${MG64_WORK}/${relative}")
    endforeach()
endif()
list(REMOVE_DUPLICATES extracted_sources)
set(linker_script "${MG64_WORK}/mariogolf64.ld")
set(undefined_functions "${MG64_WORK}/undefined_funcs_auto.txt")
set(undefined_symbols "${MG64_WORK}/undefined_syms_auto.txt")
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/extract.stamp"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MG64_WORK}"
    COMMAND "${CMAKE_COMMAND}" -E env "MG64_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "${MG64_PYTHON}" "${PROJECT_SOURCE_DIR}/tools/extract_rom.py"
        --rom "${MG64_BASE_ROM}" --stamp "${PROJECT_BINARY_DIR}/extract.stamp"
    DEPENDS "${MG64_BASE_ROM}" "${PROJECT_SOURCE_DIR}/mariogolf64.yaml"
        "${PROJECT_SOURCE_DIR}/symbol_addrs.txt" "${PROJECT_SOURCE_DIR}/ghidra_symbols.txt"
        "${PROJECT_SOURCE_DIR}/reloc_addrs.txt" "${PROJECT_SOURCE_DIR}/tools/extract_rom.py"
        "${MG64_HELPER}" ${original_sources} "${PROJECT_BINARY_DIR}/python-ready.stamp"
    BYPRODUCTS ${extracted_sources} "${linker_script}" "${undefined_functions}" "${undefined_symbols}"
        "${MG64_WORK}/mariogolf64.d" "${MG64_WORK}/extraction.yaml"
    VERBATIM)
add_custom_target(extract DEPENDS "${PROJECT_BINARY_DIR}/extract.stamp")
add_dependencies(extract mg64_python)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/mariogolf64.yaml" "${PROJECT_SOURCE_DIR}/tools/rom_manifest.py" ${original_sources})

set(elf "${PROJECT_BINARY_DIR}/mariogolf64.elf")
set(rom "${PROJECT_BINARY_DIR}/mariogolf64.z64")
set(map "${PROJECT_BINARY_DIR}/mariogolf64.map")
add_custom_command(OUTPUT "${elf}"
    COMMAND mg64_tool_ld --no-check-sections --omagic --allow-multiple-definition
        -T "${PROJECT_SOURCE_DIR}/hardware_regs.txt" -T "${undefined_functions}" -T "${undefined_symbols}"
        -T "${PROJECT_SOURCE_DIR}/extern_syms.txt" -T "${linker_script}" -Map "${map}" -o "${elf}"
    DEPENDS ${object_files} "${linker_script}" "${undefined_functions}" "${undefined_symbols}"
        "${PROJECT_SOURCE_DIR}/hardware_regs.txt" "${PROJECT_SOURCE_DIR}/extern_syms.txt" mg64_tool_ld
    BYPRODUCTS "${map}"
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" VERBATIM)
add_custom_command(OUTPUT "${rom}"
    COMMAND mg64_tool_objcopy -O binary "${elf}" "${rom}.tmp"
    COMMAND "${CMAKE_COMMAND}" -E rename "${rom}.tmp" "${rom}"
    DEPENDS "${elf}" mg64_tool_objcopy VERBATIM)
add_custom_target(rom ALL DEPENDS "${rom}")
add_dependencies(rom tools)
