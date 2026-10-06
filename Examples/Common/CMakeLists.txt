# Host-side code shared by the examples.

# Backend-neutral: command-line arguments, GLFW input forwarding, PNG screenshots, and the host of the main examples
# (window, frame loop, Carbon context) written against Example::GraphicsDevice. Every example uses it, whatever
# renderer backend it runs on, and it is compiled once. The host gets its device from CreateGraphicsDevice(), which
# the device of each backend defines (below).
add_library(CarbonExampleBase STATIC
    ExampleApp.cpp
    ExampleApp.h
    ExampleArguments.cpp
    ExampleArguments.h
    ExampleHost.cpp
    ExampleHost.h
    GlfwInput.cpp
    GlfwInput.h
    GraphicsDevice.h
    Screenshot.cpp
    Screenshot.h
)
target_include_directories(CarbonExampleBase PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CarbonExampleBase
    PUBLIC Carbon::Carbon ${CARBON_DEPS_GLFW_NAME}
    PRIVATE ${CARBON_DEPS_STB_NAME}
)
carbon_configure_target(CarbonExampleBase)

# stb is third-party code: compiled separately, without Carbon's warning flags.
add_library(CarbonExampleStb OBJECT StbImageWrite.cpp)
target_link_libraries(CarbonExampleStb PRIVATE ${CARBON_DEPS_STB_NAME})
target_sources(CarbonExampleBase PRIVATE $<TARGET_OBJECTS:CarbonExampleStb>)

# One graphics device per backend: CarbonExample<Backend> holds that backend's device, so an example linking it
# runs on that backend. They are object libraries: the device defines the function the host in CarbonExampleBase
# calls, and object files are linked whatever the order of the libraries.
# The backends are listed in CARBON_EXAMPLE_BACKENDS, in the order the executables are made.
set(carbon_example_backends)
foreach(backend IN ITEMS WebGPU Vulkan OpenGL OpenGLES DX11 DX9)
    string(TOUPPER "${backend}" option)
    if(NOT CARBON_BACKEND_${option})
        continue()
    endif()
    # A browser has only WebGL 2, through the OpenGL ES backend.
    if(EMSCRIPTEN AND NOT backend STREQUAL "OpenGLES")
        continue()
    endif()
    list(APPEND carbon_example_backends ${backend})

    set(device_source Devices/${backend}Device.cpp)
    if(backend STREQUAL "OpenGLES")
        set(device_source Devices/OpenGLDevice.cpp)
    endif()
    set(library CarbonExample${backend})
    add_library(${library} OBJECT ${device_source})
    target_include_directories(${library} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Devices")
    target_link_libraries(${library} PUBLIC CarbonExampleBase)
    if(backend STREQUAL "WebGPU")
        target_sources(${library} PRIVATE Devices/WebGPUDevice.h Devices/WebGPUSurface.cpp Devices/WebGPUSurface.h)
    elseif(backend STREQUAL "OpenGLES")
        target_compile_definitions(${library} PRIVATE CARBON_EXAMPLE_OPENGL_ES=1)
    elseif(backend STREQUAL "OpenGL")
        target_compile_definitions(${library} PRIVATE CARBON_EXAMPLE_OPENGL_ES=0)
    elseif(backend STREQUAL "DX11")
        target_link_libraries(${library} PUBLIC d3d11 dxgi)
    elseif(backend STREQUAL "DX9")
        target_link_libraries(${library} PUBLIC d3d9)
    endif()
    carbon_configure_target(${library})
endforeach()
set(CARBON_EXAMPLE_BACKENDS ${carbon_example_backends} PARENT_SCOPE)
