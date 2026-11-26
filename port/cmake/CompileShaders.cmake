# Compiles the renderer's GLSL shaders at build time, for every GPU API that
# SDL_GPU drives on the platform: SPIR-V for Vulkan, and on Apple's
# platforms also MSL, which spirv-cross translates the SPIR-V to, for Metal.
# The results are embedded in a header, so the program carries its shaders
# with it. Only Apple's builds need spirv-cross, which some Linux
# distributions, such as Fedora, don't package.

find_program(MG64_GLSLANG NAMES glslangValidator glslang REQUIRED)
if(APPLE)
    find_program(MG64_SPIRV_CROSS NAMES spirv-cross REQUIRED)
endif()

set(MG64_EMBED_SHADER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/EmbedShader.cmake")

# mg64_compile_shaders(<target> SHADERS <file>...)
#
# Each shader's stage comes from its extension, .vert or .frag. A shader
# a.frag becomes a.frag.h in the generated shaders directory, which the
# target can include as "shaders/a.frag.h", holding a_frag_spirv and
# a_frag_msl. Off Apple's platforms a_frag_msl is empty but for its
# terminating zero, and the target's MG64_SHADERS_MSL is 0.
function(mg64_compile_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SHADERS")
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
    set(headers "")

    foreach(shader IN LISTS arg_SHADERS)
        cmake_path(ABSOLUTE_PATH shader OUTPUT_VARIABLE source)
        cmake_path(GET source FILENAME file_name)
        string(MAKE_C_IDENTIFIER "${file_name}" identifier)
        set(spirv "${output_dir}/${file_name}.spv")
        set(msl "${output_dir}/${file_name}.msl")
        set(header "${output_dir}/${file_name}.h")

        add_custom_command(
            OUTPUT "${spirv}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
            COMMAND "${MG64_GLSLANG}" -V --target-env vulkan1.0 --quiet -o "${spirv}" "${source}"
            DEPENDS "${source}"
            COMMENT "Compiling ${file_name} to SPIR-V"
            VERBATIM
        )
        if(APPLE)
            add_custom_command(
                OUTPUT "${msl}"
                COMMAND "${MG64_SPIRV_CROSS}" --msl --msl-version 20100 --output "${msl}" "${spirv}"
                DEPENDS "${spirv}"
                COMMENT "Translating ${file_name} to MSL"
                VERBATIM
            )
            set(msl_arguments "-DMSL=${msl}")
            set(msl_dependency "${msl}")
        else()
            set(msl_arguments "")
            set(msl_dependency "")
        endif()
        add_custom_command(
            OUTPUT "${header}"
            COMMAND "${CMAKE_COMMAND}"
                -DSPIRV=${spirv} ${msl_arguments} -DNAME=${identifier} -DOUTPUT=${header}
                -P "${MG64_EMBED_SHADER_SCRIPT}"
            DEPENDS "${spirv}" ${msl_dependency} "${MG64_EMBED_SHADER_SCRIPT}"
            COMMENT "Embedding ${file_name}"
            VERBATIM
        )
        list(APPEND headers "${header}")
    endforeach()

    target_sources(${target} PRIVATE ${headers})
    target_compile_definitions(${target} PRIVATE MG64_SHADERS_MSL=$<BOOL:${APPLE}>)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
endfunction()
