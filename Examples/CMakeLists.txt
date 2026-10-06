# Example applications. Every example accepts
#   --screenshot <file.png>   render one settled frame offscreen, save it and exit
#   --theme light|dark
#   --scale <factor>
#   --size <width>x<height>
add_subdirectory(Common)

# One minimal integration per renderer backend, each built only when its backend is.
if(CARBON_BACKEND_WEBGPU)
    add_subdirectory(WebGPUMinimalIntegration)
endif()
if(CARBON_BACKEND_VULKAN)
    add_subdirectory(VulkanMinimalIntegration)
endif()
if(CARBON_BACKEND_OPENGL)
    add_subdirectory(OpenGLMinimalIntegration)
endif()
if(CARBON_BACKEND_OPENGLES)
    add_subdirectory(OpenGLESMinimalIntegration)
endif()
if(CARBON_BACKEND_DX11)
    add_subdirectory(DX11MinimalIntegration)
endif()

# The main examples run on WebGPU.
if(NOT CARBON_BACKEND_WEBGPU)
    message(STATUS "Carbon: skipping the main examples because CARBON_BACKEND_WEBGPU is OFF")
    return()
endif()
add_subdirectory(CustomComponent)
# The Gallery and CustomTitleBar use the extension components, so they need CarbonExtensions.
if(CARBON_BUILD_EXTENSIONS)
    add_subdirectory(Gallery)
    add_subdirectory(CustomTitleBar)
else()
    message(STATUS "Carbon: skipping the Gallery and CustomTitleBar examples because CARBON_BUILD_EXTENSIONS is OFF")
endif()
# The Reflection example needs CarbonReflection.
if(TARGET CarbonReflection)
    add_subdirectory(Reflection)
else()
    message(STATUS "Carbon: skipping the Reflection example because CarbonReflection is not built")
endif()
