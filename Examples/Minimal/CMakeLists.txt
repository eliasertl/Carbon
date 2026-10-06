# One minimal integration per renderer backend, <Backend>Minimal.cpp each, built only when its backend is, as
# Examples/<Backend>/Minimal. Unlike the other examples they do not share their source: each spells out one graphics
# API's host side by hand.
set(shaders "${CMAKE_CURRENT_BINARY_DIR}/Shaders")

if("WebGPU" IN_LIST CARBON_EXAMPLE_BACKENDS)
    add_executable(WebGPUMinimal WebGPUMinimal.cpp)
    target_link_libraries(WebGPUMinimal PRIVATE CarbonExampleWebGPU)
    carbon_configure_example(WebGPUMinimal WebGPU Minimal)
endif()

if("Vulkan" IN_LIST CARBON_EXAMPLE_BACKENDS)
    add_executable(VulkanMinimal VulkanMinimal.cpp)
    target_link_libraries(VulkanMinimal PRIVATE CarbonExampleBase)
    carbon_configure_example(VulkanMinimal Vulkan Minimal)

    # The host triangle's shaders, compiled to SPIR-V words that VulkanMinimal.cpp includes.
    foreach(shader IN ITEMS Triangle.vert Triangle.frag)
        set(output "${shaders}/${shader}.inc")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${shaders}"
            COMMAND "${Vulkan_GLSLC_EXECUTABLE}" --target-env=vulkan1.0 -mfmt=num -o "${output}"
                "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
            COMMENT "Compiling ${shader} to SPIR-V"
            VERBATIM
        )
        target_sources(VulkanMinimal PRIVATE "${output}")
    endforeach()
    target_include_directories(VulkanMinimal PRIVATE "${shaders}")
endif()

if("OpenGL" IN_LIST CARBON_EXAMPLE_BACKENDS)
    add_executable(OpenGLMinimal OpenGLMinimal.cpp)
    target_link_libraries(OpenGLMinimal PRIVATE CarbonExampleBase)
    carbon_configure_example(OpenGLMinimal OpenGL Minimal)
endif()

if("OpenGLES" IN_LIST CARBON_EXAMPLE_BACKENDS)
    # Natively, or in a browser as a page with a canvas (see carbon_configure_example).
    add_executable(OpenGLESMinimal OpenGLESMinimal.cpp)
    target_link_libraries(OpenGLESMinimal PRIVATE CarbonExampleBase)
    carbon_configure_example(OpenGLESMinimal OpenGLES Minimal)
endif()

if("DX11" IN_LIST CARBON_EXAMPLE_BACKENDS)
    add_executable(DX11Minimal DX11Minimal.cpp)
    target_link_libraries(DX11Minimal PRIVATE CarbonExampleBase d3d11 dxgi)
    carbon_configure_example(DX11Minimal DX11 Minimal)

    # The host triangle's shaders, compiled by fxc into bytecode headers that DX11Minimal.cpp includes.
    set(shader "${CMAKE_CURRENT_SOURCE_DIR}/Triangle.hlsl")
    foreach(stage IN ITEMS Vertex Pixel)
        if(stage STREQUAL "Vertex")
            set(profile vs_4_0)
        else()
            set(profile ps_4_0)
        endif()
        set(output "${shaders}/Triangle${stage}.h")
        add_custom_command(
            OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${shaders}"
            COMMAND "${CARBON_FXC_EXECUTABLE}" /nologo /T ${profile} /E ${stage}Main /O3 /WX /Fh "${output}"
                /Vn g_Triangle${stage}Shader "${shader}"
            DEPENDS "${shader}"
            COMMENT "Compiling Triangle.hlsl (${stage}Main)"
            VERBATIM
        )
        target_sources(DX11Minimal PRIVATE "${output}")
    endforeach()
    target_include_directories(DX11Minimal PRIVATE "${shaders}")
endif()

if("DX9" IN_LIST CARBON_EXAMPLE_BACKENDS)
    add_executable(DX9Minimal DX9Minimal.cpp)
    target_link_libraries(DX9Minimal PRIVATE CarbonExampleBase d3d9)
    carbon_configure_example(DX9Minimal DX9 Minimal)
endif()
