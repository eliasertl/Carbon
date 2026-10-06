add_executable(OpenGLESMinimalIntegration Main.cpp)
target_link_libraries(OpenGLESMinimalIntegration PRIVATE CarbonExampleBase)
carbon_configure_target(OpenGLESMinimalIntegration)

# In a browser: a page with a canvas, WebGL 2 (OpenGL ES 3.0), and GL functions reachable by name for Carbon's
# function table.
if(EMSCRIPTEN)
    set_target_properties(OpenGLESMinimalIntegration PROPERTIES SUFFIX ".html")
    target_link_options(OpenGLESMinimalIntegration PRIVATE
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
        -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1MB
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/Shell.html"
    )
    set_property(TARGET OpenGLESMinimalIntegration APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Shell.html")
endif()
