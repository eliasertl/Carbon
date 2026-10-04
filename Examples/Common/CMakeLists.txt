# Host-side code shared by the examples: window, device, surface, screenshots, GLFW input forwarding.
add_library(CarbonExampleCommon STATIC
    ExampleApp.cpp
    ExampleApp.h
    ExampleHost.cpp
    ExampleHost.h
    GlfwInput.cpp
    GlfwInput.h
    Surface.cpp
    Surface.h
)

target_include_directories(CarbonExampleCommon PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CarbonExampleCommon
    PUBLIC Carbon::Carbon ${CARBON_DEPS_GLFW_NAME}
    PRIVATE ${CARBON_DEPS_STB_NAME}
)
carbon_configure_target(CarbonExampleCommon)

# stb is third-party code: compiled separately, without Carbon's warning flags.
add_library(CarbonExampleStb OBJECT StbImageWrite.cpp)
target_link_libraries(CarbonExampleStb PRIVATE ${CARBON_DEPS_STB_NAME})
target_sources(CarbonExampleCommon PRIVATE $<TARGET_OBJECTS:CarbonExampleStb>)
