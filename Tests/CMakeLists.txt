include(GoogleTest)

add_executable(CarbonTests
    src/Animation/AnimationTests.cpp
    src/Animation/SpringTests.cpp
    src/Assets/EmbeddedAssetsTests.cpp
    src/Core/AllocationTests.cpp
    src/Core/ContextTests.cpp
    src/Core/IDTests.cpp
    src/Core/MathTests.cpp
    src/Core/UTF8Tests.cpp
    src/Core/VersionTests.cpp
    src/Draw/DrawListTests.cpp
    src/Draw/SquircleTests.cpp
    src/Input/CompositionTests.cpp
    src/Input/InputTests.cpp
    src/Interaction/DragDropTests.cpp
    src/Interaction/GestureTests.cpp
    src/Interaction/InteractionTests.cpp
    src/Layout/GridTests.cpp
    src/Layout/LayoutTests.cpp
    src/Layout/ScrollViewTests.cpp
    src/Main.cpp
    src/Overlay/OverlayTests.cpp
    src/Renderer/BackendColorGlyphTests.cpp
    src/Renderer/BackendCompareTests.cpp
    src/Renderer/BackendSmokeTests.cpp
    src/Renderer/RendererBackendTests.cpp
    src/Renderer/RendererTests.cpp
    src/Renderer/TextureFormatTests.cpp
    src/Style/StyleTests.cpp
    src/Style/ThemeTests.cpp
    src/Support/BackendHarness.cpp
    src/Support/BackendHarness.h
    src/Support/BackendTest.cpp
    src/Support/BackendTest.h
    src/Support/ContextTest.h
    src/Support/DX11Harness.cpp
    src/Support/DX9Harness.cpp
    src/Support/ImageCompare.cpp
    src/Support/ImageCompare.h
    src/Support/OpenGLHarness.cpp
    src/Support/TestFonts.cpp
    src/Support/TestFonts.h
    src/Support/TestScene.cpp
    src/Support/TestScene.h
    src/Support/VulkanHarness.cpp
    src/Support/WebGPUHarness.cpp
    src/Support/WidgetTest.h
    src/Text/ColorGlyphTests.cpp
    src/Text/GlyphAtlasTests.cpp
    src/Text/TextTests.cpp
    src/Widgets/TextAreaTests.cpp
    src/Widgets/TextCompositionTests.cpp
    src/Widgets/TextFieldTests.cpp
    src/Widgets/WidgetTests.cpp
)

target_include_directories(CarbonTests PRIVATE src)
target_link_libraries(CarbonTests PRIVATE Carbon::Carbon ${CARBON_DEPS_GOOGLETEST_NAME} ${CARBON_DEPS_STB_NAME})
# The OpenGL backend's tests create their context with GLFW.
if((CARBON_BACKEND_OPENGL OR CARBON_BACKEND_OPENGLES) AND NOT EMSCRIPTEN)
    target_link_libraries(CarbonTests PRIVATE ${CARBON_DEPS_GLFW_NAME})
endif()
# With Emscripten the tests run in Node: real files (for failure images), a bigger stack than the 64 KB default,
# and a heap that grows. Node has no canvas, so the GPU tests are not instantiated there.
if(EMSCRIPTEN)
    target_link_options(CarbonTests PRIVATE -sNODERAWFS=1 -sSTACK_SIZE=1MB -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1)
endif()
# The Direct3D backends' tests create their own devices.
if(CARBON_BACKEND_DX11)
    target_link_libraries(CarbonTests PRIVATE d3d11)
endif()
if(CARBON_BACKEND_DX9)
    target_link_libraries(CarbonTests PRIVATE d3d9)
endif()
# COLR glyphs are painted only with HarfBuzz's raster library; the tests of them skip without it.
if(TARGET ${CARBON_DEPS_HARFBUZZ_RASTER_NAME})
    target_compile_definitions(CarbonTests PRIVATE CARBON_TESTS_HAVE_COLOR_PAINT=1)
endif()
# Tests that compare images write the images of a failure below <build>/Tests.
target_compile_definitions(CarbonTests PRIVATE "CARBON_TESTS_OUTPUT_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")

# stb is third-party code: compiled separately, without Carbon's warning flags.
add_library(CarbonTestsStb OBJECT src/Support/StbImageWrite.cpp)
target_link_libraries(CarbonTestsStb PRIVATE ${CARBON_DEPS_STB_NAME})
target_sources(CarbonTests PRIVATE $<TARGET_OBJECTS:CarbonTestsStb>)

# The Markdown parser and documentation library of the web app (Examples/WebApp) need neither a GPU nor a browser.
# Their tests also read the documentation, whose links and images must all lead somewhere.
target_sources(CarbonTests PRIVATE
    src/WebApp/MarkdownTests.cpp
    "${PROJECT_SOURCE_DIR}/Examples/WebApp/DocLibrary.cpp"
    "${PROJECT_SOURCE_DIR}/Examples/WebApp/Markdown.cpp"
)
target_include_directories(CarbonTests PRIVATE "${PROJECT_SOURCE_DIR}/Examples/WebApp")
target_compile_definitions(CarbonTests PRIVATE "CARBON_TESTS_DOCS_DIR=\"${PROJECT_SOURCE_DIR}/Docs\"")

if(CARBON_BUILD_EXTENSIONS)
    target_sources(CarbonTests PRIVATE
        src/Extensions/ContainerTests.cpp
        src/Extensions/ControlTests.cpp
        src/Extensions/DateTests.cpp
        src/Extensions/HierarchyTests.cpp
        src/Extensions/MenuBarTests.cpp
        src/Extensions/MenuTests.cpp
        src/Extensions/NotificationTests.cpp
        src/Extensions/NumberFieldTests.cpp
        src/Extensions/TokenFieldTests.cpp
        src/Extensions/ToolbarTests.cpp
    )
    target_link_libraries(CarbonTests PRIVATE Carbon::Extensions)
    target_compile_definitions(CarbonTests PRIVATE CARBON_TESTS_HAVE_EXTENSIONS=1)
endif()

if(TARGET CarbonReflection)
    target_sources(CarbonTests PRIVATE
        src/Reflection/DisplayNameTests.cpp
        src/Reflection/EnumTests.cpp
        src/Reflection/ReflectTests.cpp
        src/Reflection/StructTests.cpp
    )
    target_link_libraries(CarbonTests PRIVATE Carbon::Reflection)
    target_compile_definitions(CarbonTests PRIVATE CARBON_TESTS_HAVE_REFLECTION=1)
endif()

carbon_configure_target(CarbonTests)

# The renderer tests create a real device when the machine has an adapter (they skip otherwise).
carbon_copy_dawn_runtime(CarbonTests)

# CarbonExtensions and the CustomComponent example may include public Carbon headers only.
get_target_property(carbon_public_include_names Carbon CARBON_PUBLIC_INCLUDE_NAMES)
list(JOIN carbon_public_include_names "\n" carbon_public_include_text)
set(carbon_public_include_file "${CMAKE_CURRENT_BINARY_DIR}/PublicHeaders.txt")
file(WRITE "${carbon_public_include_file}" "${carbon_public_include_text}\n")
foreach(boundary IN ITEMS Extensions/src Examples/CustomComponent)
    string(REPLACE "/" "." boundary_name "${boundary}")
    add_test(NAME PublicApiBoundary.${boundary_name}
        COMMAND "${CMAKE_COMMAND}"
            "-DCHECK_DIR=${PROJECT_SOURCE_DIR}/${boundary}"
            "-DALLOWED_FILE=${carbon_public_include_file}"
            -P "${PROJECT_SOURCE_DIR}/Framework/CMake/CheckPublicIncludes.cmake"
    )
endforeach()
# CarbonReflection may include public Carbon and public CarbonExtensions headers.
if(TARGET CarbonReflection)
    get_target_property(carbon_extensions_include_names CarbonExtensions CARBON_PUBLIC_INCLUDE_NAMES)
    list(JOIN carbon_extensions_include_names "\n" carbon_extensions_include_text)
    set(carbon_reflection_allowed_file "${CMAKE_CURRENT_BINARY_DIR}/ReflectionAllowedHeaders.txt")
    file(WRITE "${carbon_reflection_allowed_file}" "${carbon_public_include_text}\n${carbon_extensions_include_text}\n")
    add_test(NAME PublicApiBoundary.Reflection.src
        COMMAND "${CMAKE_COMMAND}"
            "-DCHECK_DIR=${PROJECT_SOURCE_DIR}/Reflection/src"
            "-DALLOWED_FILE=${carbon_reflection_allowed_file}"
            "-DOWN_PREFIX=Carbon/Reflection/"
            -P "${PROJECT_SOURCE_DIR}/Framework/CMake/CheckPublicIncludes.cmake"
    )
endif()
# Only the renderer backends may use a graphics API; everything else stays GPU-free.
add_test(NAME BackendIsolation
    COMMAND "${CMAKE_COMMAND}"
        "-DCHECK_DIRS=${PROJECT_SOURCE_DIR}/Framework/src;${PROJECT_SOURCE_DIR}/Extensions/src;${PROJECT_SOURCE_DIR}/Reflection/src"
        "-DEXCLUDE_DIR=${PROJECT_SOURCE_DIR}/Framework/src/Carbon/Backends"
        -P "${PROJECT_SOURCE_DIR}/Framework/CMake/CheckBackendIsolation.cmake"
)

# A public header must not pull in an internal one either.
add_test(NAME PublicApiBoundary.PublicHeaders
    COMMAND "${CMAKE_COMMAND}"
        "-DCHECK_DIR=${PROJECT_SOURCE_DIR}/Framework/src"
        "-DALLOWED_FILE=${carbon_public_include_file}"
        "-DCHECK_LIST_FILE=${carbon_public_include_file}"
        -P "${PROJECT_SOURCE_DIR}/Framework/CMake/CheckPublicIncludes.cmake"
)

# How CTest runs CarbonTests. Starting the executable costs more than most tests, so `ctest --parallel` is what
# makes the suite fast; everything below is safe to run in parallel.
if(EMSCRIPTEN)
    # In Node, starting the executable means compiling the module, and there are no GPU tests: a few entries that
    # each run the suites of a part of the alphabet.
    set(carbon_test_shards
        "A*:B*:C*:D*"
        "E*:F*:G*:H*:I*:J*:K*:L*"
        "M*:N*:O*:P*:Q*:R*"
        "S*:T*:U*:V*:W*:X*:Y*:Z*"
    )
    set(carbon_test_shard 1)
    foreach(filter IN LISTS carbon_test_shards)
        add_test(NAME CarbonTests.Part${carbon_test_shard} COMMAND CarbonTests "--gtest_filter=${filter}")
        set_tests_properties(CarbonTests.Part${carbon_test_shard} PROPERTIES SKIP_RETURN_CODE 77)
        math(EXPR carbon_test_shard "${carbon_test_shard} + 1")
    endforeach()
else()
    # The tests that need no GPU are entries of their own. PRE_TEST discovery keeps the build independent of
    # being able to run the test binary.
    gtest_discover_tests(CarbonTests DISCOVERY_MODE PRE_TEST TEST_FILTER "-Backends/*")

    # The tests of the renderer backends (Backends/<Suite>.<Test>/<Backend>) run as one entry per backend: its
    # tests share one device, which takes longer to create than a test takes to run, and the entries take turns
    # on the GPU. An entry whose backend has no device on this machine is shown as skipped. The names are those
    # of GetCompiledBackends in src/Support/BackendHarness.cpp.
    set(carbon_test_backends)
    if(CARBON_BACKEND_WEBGPU)
        list(APPEND carbon_test_backends WebGPU)
    endif()
    if(CARBON_BACKEND_OPENGL)
        list(APPEND carbon_test_backends OpenGL)
    endif()
    if(CARBON_BACKEND_OPENGLES)
        list(APPEND carbon_test_backends OpenGLES)
    endif()
    if(CARBON_BACKEND_DX11)
        list(APPEND carbon_test_backends DX11)
    endif()
    if(CARBON_BACKEND_DX9)
        list(APPEND carbon_test_backends DX9 DX9Invalidate)
    endif()
    if(CARBON_BACKEND_VULKAN)
        list(APPEND carbon_test_backends Vulkan VulkanRenderPass)
    endif()
    foreach(backend IN LISTS carbon_test_backends)
        add_test(NAME Backends.${backend} COMMAND CarbonTests "--gtest_filter=Backends/*/${backend}")
        set_tests_properties(Backends.${backend} PROPERTIES RESOURCE_LOCK CarbonGpu SKIP_RETURN_CODE 77)
    endforeach()
endif()
