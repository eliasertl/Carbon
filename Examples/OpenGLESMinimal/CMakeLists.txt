# Natively, or in a browser as a page with a canvas (see carbon_configure_example).
add_executable(OpenGLESMinimal Main.cpp)
target_link_libraries(OpenGLESMinimal PRIVATE CarbonExampleBase)
carbon_configure_example(OpenGLESMinimal OpenGLES)
