# Host-side code shared by the examples.

# Backend-neutral: command-line arguments, GLFW input forwarding, PNG screenshots. Every example uses it, whatever
# renderer backend it runs on.
add_library(CarbonExampleBase STATIC
    ExampleArguments.cpp
    ExampleArguments.h
    GlfwInput.cpp
    GlfwInput.h
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

# The WebGPU host of the main examples: window, device, surface, offscreen screenshot target, frame loop.
if(CARBON_BACKEND_WEBGPU)
    add_library(CarbonExampleCommon STATIC
        ExampleApp.cpp
        ExampleApp.h
        ExampleHost.cpp
        ExampleHost.h
        Surface.cpp
        Surface.h
    )
    target_link_libraries(CarbonExampleCommon PUBLIC CarbonExampleBase)
    carbon_configure_target(CarbonExampleCommon)
endif()
