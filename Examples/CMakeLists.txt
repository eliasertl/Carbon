# Example applications. Every example accepts
#   --screenshot <file.png>   render one settled frame offscreen, save it and exit
#   --theme light|dark
#   --scale <factor>
#   --size <width>x<height>
#
# Each example is built once per renderer backend, named <Backend><Example>: WebGPUGallery, VulkanGallery, ...,
# DX9CustomTitleBar. The <Backend>Minimal examples spell out one backend's integration each; the others are written
# once against Examples/Common and link the graphics device of their backend. In a browser (Emscripten) there is
# only the OpenGL ES backend, over WebGL 2, and no CustomTitleBar: a page has no window to move.
add_subdirectory(Common)

# Settings every example executable gets: warnings, Dawn's runtime next to WebGPU ones, and a web page in a browser.
function(carbon_configure_example target backend)
    carbon_configure_target(${target})
    if(backend STREQUAL "WebGPU")
        carbon_copy_dawn_runtime(${target})
    endif()
    # In a browser: a page with a canvas, WebGL 2 (OpenGL ES 3.0), and GL functions reachable by name for Carbon's
    # function table.
    if(EMSCRIPTEN)
        set(shell "${PROJECT_SOURCE_DIR}/Examples/Common/Shell.html")
        set_target_properties(${target} PROPERTIES SUFFIX ".html")
        target_link_options(${target} PRIVATE
            -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
            -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1MB
            "--shell-file=${shell}"
        )
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${shell}")
    endif()
endfunction()

# carbon_add_backend_examples(<Name> SOURCES <files>... [LIBRARIES <targets>...] [NO_WEB])
# Makes the executable <Backend><Name> for every backend in CARBON_EXAMPLE_BACKENDS.
function(carbon_add_backend_examples name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "NO_WEB" "" "SOURCES;LIBRARIES")
    if(EMSCRIPTEN AND arg_NO_WEB)
        return()
    endif()
    foreach(backend IN LISTS CARBON_EXAMPLE_BACKENDS)
        set(target ${backend}${name})
        add_executable(${target} ${arg_SOURCES})
        target_link_libraries(${target} PRIVATE CarbonExample${backend} ${arg_LIBRARIES})
        carbon_configure_example(${target} ${backend})
    endforeach()
endfunction()

# One minimal integration per renderer backend, each built only when its backend is.
foreach(backend IN LISTS CARBON_EXAMPLE_BACKENDS)
    add_subdirectory(${backend}Minimal)
endforeach()

add_subdirectory(CustomComponent)
# The Gallery and CustomTitleBar use the extension components, so they need CarbonExtensions.
if(CARBON_BUILD_EXTENSIONS)
    add_subdirectory(Gallery)
    add_subdirectory(CustomTitleBar)
else()
    message(STATUS "Carbon: skipping the Gallery and CustomTitleBar examples because CARBON_BUILD_EXTENSIONS is OFF")
endif()
# The Reflection example needs CarbonReflection. It runs on the first backend of CARBON_EXAMPLE_BACKENDS.
if(TARGET CarbonReflection AND CARBON_EXAMPLE_BACKENDS)
    add_subdirectory(Reflection)
else()
    message(STATUS "Carbon: skipping the Reflection example because CarbonReflection is not built")
endif()
