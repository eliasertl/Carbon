add_executable(DX11MinimalIntegration Main.cpp)
target_link_libraries(DX11MinimalIntegration PRIVATE CarbonExampleBase d3d11 dxgi)
carbon_configure_target(DX11MinimalIntegration)

# The host triangle's shaders, compiled by fxc into bytecode headers that Main.cpp includes.
set(shader "${CMAKE_CURRENT_SOURCE_DIR}/Triangle.hlsl")
foreach(stage IN ITEMS Vertex Pixel)
    if(stage STREQUAL "Vertex")
        set(profile vs_4_0)
    else()
        set(profile ps_4_0)
    endif()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/Shaders/Triangle${stage}.h")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/Shaders"
        COMMAND "${CARBON_FXC_EXECUTABLE}" /nologo /T ${profile} /E ${stage}Main /O3 /WX /Fh "${output}"
            /Vn g_Triangle${stage}Shader "${shader}"
        DEPENDS "${shader}"
        COMMENT "Compiling Triangle.hlsl (${stage}Main)"
        VERBATIM
    )
    target_sources(DX11MinimalIntegration PRIVATE "${output}")
endforeach()
target_include_directories(DX11MinimalIntegration PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
