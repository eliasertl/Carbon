# Carbon's benchmarks: the CarbonBenchmarks executable, built on Google Benchmark. It measures steady-state frames
# of the headless core (frame time, heap allocations, draw data) and each compiled-in backend's render function.
# Docs/Optimizations.md explains the scenarios and how to run them.
if(EMSCRIPTEN)
    message(STATUS "Carbon: skipping the benchmarks, which measure the desktop library")
    return()
endif()
if(NOT CARBON_BUILD_EXTENSIONS)
    message(STATUS "Carbon: skipping the benchmarks because CARBON_BUILD_EXTENSIONS is OFF")
    return()
endif()

add_executable(CarbonBenchmarks
    src/Backends/BackendBenchmarks.cpp
    src/Charts/ChartBenchmarks.cpp
    src/Controls/ControlBenchmarks.cpp
    src/Gallery/GalleryBenchmarks.cpp
    src/Idle/IdleBenchmarks.cpp
    src/Layout/ContainerBenchmarks.cpp
    src/Main.cpp
    src/Rows/RowBenchmarks.cpp
    src/Support/AllocationCounter.cpp
    src/Support/AllocationCounter.h
    src/Support/FrameBenchmark.cpp
    src/Support/FrameBenchmark.h
    src/Text/GlyphAtlasBenchmarks.cpp
    src/Text/TextBenchmarks.cpp
)
target_include_directories(CarbonBenchmarks PRIVATE src)
target_link_libraries(CarbonBenchmarks PRIVATE Carbon::Carbon Carbon::Extensions ${CARBON_DEPS_GOOGLEBENCHMARK_NAME})

# The Gallery frame is the Gallery's own page code, compiled into the benchmark.
set(carbon_gallery_dir "${PROJECT_SOURCE_DIR}/Examples/Gallery")
target_sources(CarbonBenchmarks PRIVATE
    "${carbon_gallery_dir}/CorePages.cpp"
    "${carbon_gallery_dir}/ExtensionPages.cpp"
    "${carbon_gallery_dir}/GalleryState.h"
    "${carbon_gallery_dir}/Pages.h"
)
target_include_directories(CarbonBenchmarks PRIVATE "${carbon_gallery_dir}")
if(TARGET CarbonReflection)
    target_sources(CarbonBenchmarks PRIVATE "${carbon_gallery_dir}/ReflectionPage.cpp")
    target_link_libraries(CarbonBenchmarks PRIVATE Carbon::Reflection)
    target_compile_definitions(CarbonBenchmarks PRIVATE CARBON_GALLERY_HAVE_REFLECTION=1)
endif()

# The backend benchmarks drive each backend offscreen through the harnesses of the renderer tests.
set(carbon_test_support_dir "${PROJECT_SOURCE_DIR}/Tests/src")
target_sources(CarbonBenchmarks PRIVATE
    "${carbon_test_support_dir}/Support/BackendHarness.cpp"
    "${carbon_test_support_dir}/Support/BackendHarness.h"
    "${carbon_test_support_dir}/Support/DX11Harness.cpp"
    "${carbon_test_support_dir}/Support/DX9Harness.cpp"
    "${carbon_test_support_dir}/Support/OpenGLHarness.cpp"
    "${carbon_test_support_dir}/Support/TestScene.cpp"
    "${carbon_test_support_dir}/Support/TestScene.h"
    "${carbon_test_support_dir}/Support/VulkanHarness.cpp"
    "${carbon_test_support_dir}/Support/WebGPUHarness.cpp"
)
target_include_directories(CarbonBenchmarks PRIVATE "${carbon_test_support_dir}")
if(CARBON_BACKEND_OPENGL OR CARBON_BACKEND_OPENGLES)
    target_link_libraries(CarbonBenchmarks PRIVATE ${CARBON_DEPS_GLFW_NAME})
endif()
if(CARBON_BACKEND_DX11)
    target_link_libraries(CarbonBenchmarks PRIVATE d3d11)
endif()
if(CARBON_BACKEND_DX9)
    target_link_libraries(CarbonBenchmarks PRIVATE d3d9)
endif()

carbon_configure_target(CarbonBenchmarks)
# The WebGPU benchmarks create a real device when the machine has an adapter.
carbon_copy_dawn_runtime(CarbonBenchmarks)
