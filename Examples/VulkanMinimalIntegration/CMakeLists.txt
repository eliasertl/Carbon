add_executable(VulkanMinimalIntegration Main.cpp)
target_link_libraries(VulkanMinimalIntegration PRIVATE CarbonExampleBase)
carbon_configure_target(VulkanMinimalIntegration)

# The host triangle's shaders, compiled to SPIR-V words that Main.cpp includes.
foreach(shader IN ITEMS Triangle.vert Triangle.frag)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/Shaders/${shader}.inc")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" --target-env=vulkan1.0 -mfmt=num -o "${output}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
        COMMENT "Compiling ${shader} to SPIR-V"
        VERBATIM
    )
    target_sources(VulkanMinimalIntegration PRIVATE "${output}")
endforeach()
target_include_directories(VulkanMinimalIntegration PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
