add_executable(VulkanMinimal Main.cpp)
target_link_libraries(VulkanMinimal PRIVATE CarbonExampleBase)
carbon_configure_example(VulkanMinimal Vulkan)

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
    target_sources(VulkanMinimal PRIVATE "${output}")
endforeach()
target_include_directories(VulkanMinimal PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
