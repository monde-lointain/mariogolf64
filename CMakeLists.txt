cmake_minimum_required(VERSION 3.30)
project(MarioGolf64 VERSION 0.1.0 LANGUAGES C)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset us")
endif()
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    message(FATAL_ERROR "The pinned host toolchain supports Linux x86_64 (including WSL).")
endif()
find_program(MG64_UV NAMES uv REQUIRED)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "-m32 -static")
check_c_source_compiles("int main(void) { return 0; }" MG64_HOST_HAS_32BIT_LIBC)
cmake_pop_check_state()
if(NOT MG64_HOST_HAS_32BIT_LIBC)
    message(FATAL_ERROR "KMC GCC requires 32-bit host libraries. Install gcc-multilib and libc6-dev-i386.")
endif()

add_subdirectory(third_party)
include(cmake/Rom.cmake)

# Machine-readable host-tool locations are also consumed by isolated compile,
# diff, permuter, and ranker tooling. No second set of compiler paths or flags.
set(tools_json "{}")
function(mg64_json_path key value)
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(JSON tools_json SET "${tools_json}" "${key}" "\"${value}\"")
    set(tools_json "${tools_json}" PARENT_SCOPE)
endfunction()
mg64_json_path(root "${PROJECT_SOURCE_DIR}")
mg64_json_path(build_dir "${PROJECT_BINARY_DIR}")
mg64_json_path(work_dir "${MG64_WORK}")
mg64_json_path(python "${MG64_PYTHON}")
foreach(tool IN ITEMS gcc kmc_as cpp as ld objcopy strip objdump nm)
    mg64_json_path(${tool} "${MG64_TOOL_${tool}}")
endforeach()
if(MG64_NONMATCHING)
    string(JSON tools_json SET "${tools_json}" nonmatching true)
else()
    string(JSON tools_json SET "${tools_json}" nonmatching false)
endif()
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/build-tools.json" CONTENT "${tools_json}\n")

include(CTest)
if(BUILD_TESTING)
    add_test(NAME mg64.rom COMMAND "${PROJECT_SOURCE_DIR}/tools/verify-rom.sh" --build-dir "${PROJECT_BINARY_DIR}")
    set_tests_properties(mg64.rom PROPERTIES RESOURCE_LOCK mg64_rom_build TIMEOUT 1200 LABELS rom)
    add_test(NAME mg64.tooling COMMAND "${CMAKE_COMMAND}" -E env "MG64_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "UV_PROJECT_ENVIRONMENT=${PROJECT_SOURCE_DIR}/.venv"
        "${MG64_UV}" run --locked --project "${PROJECT_SOURCE_DIR}"
        python -m pytest "${PROJECT_SOURCE_DIR}/tests/tooling")
    set_tests_properties(mg64.tooling PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        RESOURCE_LOCK mg64_rom_build LABELS tooling)
endif()

foreach(command IN ITEMS init refresh start stop status)
    add_custom_target(ghidra-${command}
        COMMAND "${CMAKE_COMMAND}" -E env "MG64_BUILD_DIR=${PROJECT_BINARY_DIR}"
            "UV_PROJECT_ENVIRONMENT=${PROJECT_SOURCE_DIR}/.venv"
            "${MG64_UV}" run --locked --project "${PROJECT_SOURCE_DIR}"
            python "${PROJECT_SOURCE_DIR}/tools/ghidra.py" ${command}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" USES_TERMINAL VERBATIM)
endforeach()
add_dependencies(ghidra-init extract)
add_custom_target(sync-names
    COMMAND "${CMAKE_COMMAND}" -E env "MG64_BUILD_DIR=${PROJECT_BINARY_DIR}"
        "UV_PROJECT_ENVIRONMENT=${PROJECT_SOURCE_DIR}/.venv"
        "${MG64_UV}" run --locked --project "${PROJECT_SOURCE_DIR}"
        python "${PROJECT_SOURCE_DIR}/tools/ghidra.py" sync-names
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" USES_TERMINAL VERBATIM)
foreach(sweep IN ITEMS coddog-sweep coddog-sweep-nusys coddog-sweep-audio)
    set(sweep_env "${CMAKE_COMMAND}" -E env "MG64_BUILD_DIR=${PROJECT_BINARY_DIR}")
    if(sweep STREQUAL "coddog-sweep")
        set(commands COMMAND ${sweep_env} "${PROJECT_SOURCE_DIR}/tools/coddog_sweep.sh")
    elseif(sweep STREQUAL "coddog-sweep-nusys")
        set(commands COMMAND ${sweep_env} "${PROJECT_SOURCE_DIR}/tools/build_nusys_ref.sh"
                     COMMAND ${sweep_env} "${PROJECT_SOURCE_DIR}/tools/nusys_sweep.sh")
    else()
        set(commands COMMAND ${sweep_env} "${PROJECT_SOURCE_DIR}/tools/build_audio_refs.sh"
                     COMMAND ${sweep_env} "${PROJECT_SOURCE_DIR}/tools/audio_sweep.sh"
                     COMMAND ${sweep_env} "${MG64_PYTHON}" "${PROJECT_SOURCE_DIR}/tools/audio_pin.py")
    endif()
    add_custom_target(${sweep} ${commands}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}" USES_TERMINAL VERBATIM)
    add_dependencies(${sweep} rom)
endforeach()
