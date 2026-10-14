include(ExternalProject)
find_program(MG64_HOST_MAKE NAMES gmake make REQUIRED)
find_program(MG64_HOST_CPP NAMES cpp REQUIRED)
set(MG64_TOOL_JOBS 4 CACHE STRING "Parallel jobs within each vendored host-tool build")
set(MG64_TOOLCHAIN "${PROJECT_BINARY_DIR}/toolchain")
set(MG64_KMC_DIR "${MG64_TOOLCHAIN}/kmc")
set(MG64_LEGACY_HOST_FLAGS "-std=gnu89 -fcommon -Wno-error -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types")

ExternalProject_Add(mg64_kmc_gcc
    URL "${CMAKE_CURRENT_SOURCE_DIR}/dist/mips-gcc-2.7.2-43d1cdb.tar.gz"
    URL_HASH SHA256=dc783433b18121f92c84af3c3bcaaa7a0230613a83c401de88d3b3aa25226fb4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX "${PROJECT_BINARY_DIR}/vendor/kmc-gcc"
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --target=mips-mips-gnu
        --prefix=${MG64_TOOLCHAIN} --with-gnu-as --host=i386-pc-linux --build=i386-pc-linux
    BUILD_COMMAND "${CMAKE_COMMAND}" -E touch <SOURCE_DIR>/c-parse.c
    COMMAND "${MG64_HOST_MAKE}" -j${MG64_TOOL_JOBS}
        "CC=${CMAKE_C_COMPILER}" "CFLAGS=${MG64_LEGACY_HOST_FLAGS} -m32 -static" xgcc cc1 cpp cccp
    INSTALL_COMMAND "${CMAKE_COMMAND}" -E make_directory "${MG64_KMC_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy <BINARY_DIR>/xgcc "${MG64_KMC_DIR}/gcc"
    COMMAND "${CMAKE_COMMAND}" -E copy <BINARY_DIR>/cc1 <BINARY_DIR>/cpp <BINARY_DIR>/cccp "${MG64_KMC_DIR}"
    BUILD_BYPRODUCTS <BINARY_DIR>/xgcc <BINARY_DIR>/cc1 <BINARY_DIR>/cpp <BINARY_DIR>/cccp
    INSTALL_BYPRODUCTS "${MG64_KMC_DIR}/gcc" "${MG64_KMC_DIR}/cc1" "${MG64_KMC_DIR}/cpp" "${MG64_KMC_DIR}/cccp"
    LOG_CONFIGURE TRUE LOG_BUILD TRUE LOG_INSTALL TRUE LOG_OUTPUT_ON_FAILURE TRUE)

set(MG64_KMC_BINUTILS_HOST_FLAGS "${MG64_LEGACY_HOST_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")
ExternalProject_Add(mg64_kmc_binutils
    URL "${CMAKE_CURRENT_SOURCE_DIR}/dist/mips-binutils-2.6-54514de.tar.gz"
    URL_HASH SHA256=edefc8c1d77c45f28b1220fa12cdba0fb396c4c5a255f09887e6042f928c53c3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX "${PROJECT_BINARY_DIR}/vendor/kmc-binutils"
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env
        "CC=${CMAKE_C_COMPILER}" "CFLAGS=${MG64_KMC_BINUTILS_HOST_FLAGS}"
        <SOURCE_DIR>/configure --target=mips-kmc-elf --prefix=${MG64_TOOLCHAIN}
        --host=i386-pc-linux --build=i386-pc-linux --disable-gprof --disable-gdb --disable-werror
    BUILD_COMMAND "${MG64_HOST_MAKE}" -j${MG64_TOOL_JOBS} "CFLAGS=${MG64_KMC_BINUTILS_HOST_FLAGS}" -C bfd
    COMMAND "${MG64_HOST_MAKE}" -j${MG64_TOOL_JOBS} "CFLAGS=${MG64_KMC_BINUTILS_HOST_FLAGS}" -C opcodes
    COMMAND "${MG64_HOST_MAKE}" -j${MG64_TOOL_JOBS} "CFLAGS=${MG64_KMC_BINUTILS_HOST_FLAGS}" -C libiberty
    COMMAND "${MG64_HOST_MAKE}" -j${MG64_TOOL_JOBS} "CFLAGS=${MG64_KMC_BINUTILS_HOST_FLAGS}" -C gas
    INSTALL_COMMAND "${CMAKE_COMMAND}" -E make_directory "${MG64_KMC_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy <BINARY_DIR>/gas/as.new "${MG64_KMC_DIR}/as"
    BUILD_BYPRODUCTS <BINARY_DIR>/gas/as.new
    INSTALL_BYPRODUCTS "${MG64_KMC_DIR}/as"
    LOG_CONFIGURE TRUE LOG_BUILD TRUE LOG_INSTALL TRUE LOG_OUTPUT_ON_FAILURE TRUE)

set(MG64_MODERN_TOOLS)
foreach(tool IN ITEMS as ld objcopy strip objdump nm)
    list(APPEND MG64_MODERN_TOOLS "${MG64_TOOLCHAIN}/bin/mips-linux-gnu-${tool}")
endforeach()
ExternalProject_Add(mg64_binutils
    URL "${CMAKE_CURRENT_SOURCE_DIR}/dist/binutils-2.45.1.tar.xz"
    URL_HASH SHA256=5fe101e6fe9d18fdec95962d81ed670fdee5f37e3f48f0bef87bddf862513aa5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    PREFIX "${PROJECT_BINARY_DIR}/vendor/binutils"
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env "CC=${CMAKE_C_COMPILER}"
        <SOURCE_DIR>/configure --target=mips-linux-gnu --prefix=${MG64_TOOLCHAIN}
        --disable-nls --disable-werror --disable-gdb --disable-gprof --disable-gprofng
        --disable-sim --disable-multilib --without-zstd --without-debuginfod
    BUILD_COMMAND "${MG64_HOST_MAKE}" -j${MG64_TOOL_JOBS} MAKEINFO=true all-binutils all-gas all-ld
    INSTALL_COMMAND "${MG64_HOST_MAKE}" MAKEINFO=true install-binutils install-gas install-ld
    INSTALL_BYPRODUCTS ${MG64_MODERN_TOOLS}
    LOG_CONFIGURE TRUE LOG_BUILD TRUE LOG_INSTALL TRUE LOG_OUTPUT_ON_FAILURE TRUE)

set(MG64_PYTHON "${PROJECT_SOURCE_DIR}/.venv/bin/python3")
add_custom_target(mg64_python
    COMMAND "${CMAKE_COMMAND}" -E env "UV_PROJECT_ENVIRONMENT=${PROJECT_SOURCE_DIR}/.venv"
        "${MG64_UV}" sync --locked --project "${PROJECT_SOURCE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/uv.lock" "${PROJECT_BINARY_DIR}/python-ready.stamp"
    DEPENDS "${PROJECT_SOURCE_DIR}/pyproject.toml" "${PROJECT_SOURCE_DIR}/uv.lock"
        "${MG64_UV}"
    BYPRODUCTS "${PROJECT_BINARY_DIR}/python-ready.stamp"
    COMMENT "Synchronizing the uv-managed Python environment"
    VERBATIM)
add_custom_target(tools)
add_dependencies(tools mg64_python mg64_kmc_gcc mg64_kmc_binutils mg64_binutils)

foreach(tool IN ITEMS gcc kmc_as cpp as ld objcopy strip objdump nm)
    add_executable(mg64_tool_${tool} IMPORTED GLOBAL)
    if(tool STREQUAL "gcc")
        set(location "${MG64_KMC_DIR}/gcc")
        add_dependencies(mg64_tool_${tool} mg64_kmc_gcc)
    elseif(tool STREQUAL "kmc_as")
        set(location "${MG64_KMC_DIR}/as")
        add_dependencies(mg64_tool_${tool} mg64_kmc_binutils)
    elseif(tool STREQUAL "cpp")
        set(location "${MG64_HOST_CPP}")
    else()
        set(location "${MG64_TOOLCHAIN}/bin/mips-linux-gnu-${tool}")
        add_dependencies(mg64_tool_${tool} mg64_binutils)
    endif()
    set_property(TARGET mg64_tool_${tool} PROPERTY IMPORTED_LOCATION "${location}")
    set(MG64_TOOL_${tool} "${location}" PARENT_SCOPE)
endforeach()
set(MG64_PYTHON "${MG64_PYTHON}" PARENT_SCOPE)
