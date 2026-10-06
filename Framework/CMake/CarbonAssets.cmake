# Build-time generation of embedded assets. Everything is written below CARBON_GENERATED_DIR in the build tree.

set(CARBON_ASSET_SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Embeds FILE into TARGET as Carbon::Internal::g_<SYMBOL>Data / g_<SYMBOL>Size.
function(carbon_embed_asset TARGET SYMBOL FILE)
    if(NOT EXISTS "${FILE}")
        message(FATAL_ERROR
            "Carbon: asset '${FILE}' is missing.\n"
            "Run: git submodule update --init --recursive")
    endif()
    set(CARBON_OUTPUT "${CARBON_GENERATED_DIR}/Carbon/Assets/${SYMBOL}.cpp")
    add_custom_command(
        OUTPUT "${CARBON_OUTPUT}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${FILE}"
            "-DOUTPUT=${CARBON_OUTPUT}"
            "-DSYMBOL=${SYMBOL}"
            -P "${CARBON_ASSET_SCRIPT_DIR}/EmbedAsset.cmake"
        DEPENDS "${FILE}" "${CARBON_ASSET_SCRIPT_DIR}/EmbedAsset.cmake"
        COMMENT "Carbon: embedding ${SYMBOL}"
        VERBATIM
    )
    target_sources(${TARGET} PRIVATE "${CARBON_OUTPUT}")
endfunction()

# Compiles a GLSL shader to SPIR-V with glslc (Vulkan_GLSLC_EXECUTABLE) into OUTPUT, a comma-separated list of
# 32-bit words that a source file of TARGET includes inside an array initializer. The output's folder is on
# TARGET's private include path, so it is included from the generated tree's root.
function(carbon_compile_spirv TARGET SHADER OUTPUT)
    get_filename_component(output_dir "${OUTPUT}" DIRECTORY)
    get_filename_component(shader_name "${SHADER}" NAME)
    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" --target-env=vulkan1.0 -O -mfmt=num -o "${OUTPUT}" "${SHADER}"
        DEPENDS "${SHADER}"
        COMMENT "Carbon: compiling ${shader_name} to SPIR-V"
        VERBATIM
    )
    target_sources(${TARGET} PRIVATE "${OUTPUT}")
    target_include_directories(${TARGET} PRIVATE "${CARBON_GENERATED_DIR}")
endfunction()

# Adds the rule that generates Carbon/Text/Icons.h from Phosphor's stylesheet. The caller lists the header in the
# target's public header file set, which makes it a source of the target.
function(carbon_generate_icons STYLESHEET)
    if(NOT EXISTS "${STYLESHEET}")
        message(FATAL_ERROR
            "Carbon: '${STYLESHEET}' is missing.\n"
            "Run: git submodule update --init --recursive")
    endif()
    set(CARBON_OUTPUT "${CARBON_GENERATED_DIR}/Carbon/Text/Icons.h")
    add_custom_command(
        OUTPUT "${CARBON_OUTPUT}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${STYLESHEET}"
            "-DOUTPUT=${CARBON_OUTPUT}"
            -P "${CARBON_ASSET_SCRIPT_DIR}/GenerateIcons.cmake"
        DEPENDS "${STYLESHEET}" "${CARBON_ASSET_SCRIPT_DIR}/GenerateIcons.cmake"
        COMMENT "Carbon: generating icon constants"
        VERBATIM
    )
endfunction()
