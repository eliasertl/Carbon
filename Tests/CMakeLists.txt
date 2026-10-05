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
    src/Input/InputTests.cpp
    src/Interaction/InteractionTests.cpp
    src/Layout/GridTests.cpp
    src/Layout/LayoutTests.cpp
    src/Layout/ScrollViewTests.cpp
    src/Overlay/OverlayTests.cpp
    src/Renderer/RendererTests.cpp
    src/Style/StyleTests.cpp
    src/Style/ThemeTests.cpp
    src/Support/ContextTest.h
    src/Support/GpuTest.cpp
    src/Support/GpuTest.h
    src/Support/WidgetTest.h
    src/Text/GlyphAtlasTests.cpp
    src/Text/TextTests.cpp
    src/Widgets/TextFieldTests.cpp
    src/Widgets/WidgetTests.cpp
)

target_include_directories(CarbonTests PRIVATE src)
target_link_libraries(CarbonTests PRIVATE Carbon::Carbon ${CARBON_DEPS_GOOGLETEST_NAME})

if(CARBON_BUILD_EXTENSIONS)
    target_sources(CarbonTests PRIVATE
        src/Extensions/ContainerTests.cpp
        src/Extensions/ControlTests.cpp
        src/Extensions/HierarchyTests.cpp
        src/Extensions/MenuBarTests.cpp
        src/Extensions/MenuTests.cpp
        src/Extensions/NotificationTests.cpp
    )
    target_link_libraries(CarbonTests PRIVATE Carbon::Extensions)
    target_compile_definitions(CarbonTests PRIVATE CARBON_TESTS_HAVE_EXTENSIONS=1)
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
# A public header must not pull in an internal one either.
add_test(NAME PublicApiBoundary.PublicHeaders
    COMMAND "${CMAKE_COMMAND}"
        "-DCHECK_DIR=${PROJECT_SOURCE_DIR}/Framework/src"
        "-DALLOWED_FILE=${carbon_public_include_file}"
        "-DCHECK_LIST_FILE=${carbon_public_include_file}"
        -P "${PROJECT_SOURCE_DIR}/Framework/CMake/CheckPublicIncludes.cmake"
)

# PRE_TEST discovery keeps the build independent of being able to run the test binary.
gtest_discover_tests(CarbonTests DISCOVERY_MODE PRE_TEST)
