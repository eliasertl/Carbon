include(GoogleTest)

add_executable(CarbonTests
    src/Assets/EmbeddedAssetsTests.cpp
    src/Core/ContextTests.cpp
    src/Core/IDTests.cpp
    src/Core/MathTests.cpp
    src/Core/UTF8Tests.cpp
    src/Core/VersionTests.cpp
    src/Draw/DrawListTests.cpp
    src/Draw/SquircleTests.cpp
    src/Input/InputTests.cpp
    src/Renderer/RendererTests.cpp
    src/Support/ContextTest.h
    src/Support/GpuTest.cpp
    src/Support/GpuTest.h
    src/Text/GlyphAtlasTests.cpp
    src/Text/TextTests.cpp
)

target_include_directories(CarbonTests PRIVATE src)
target_link_libraries(CarbonTests PRIVATE Carbon::Carbon ${CARBON_DEPS_GOOGLETEST_NAME})

if(CARBON_BUILD_EXTENSIONS)
    target_link_libraries(CarbonTests PRIVATE Carbon::Extensions)
    target_compile_definitions(CarbonTests PRIVATE CARBON_TESTS_HAVE_EXTENSIONS=1)
endif()

carbon_configure_target(CarbonTests)

# The renderer tests create a real device when the machine has an adapter (they skip otherwise).
carbon_copy_dawn_runtime(CarbonTests)

# PRE_TEST discovery keeps the build independent of being able to run the test binary.
gtest_discover_tests(CarbonTests DISCOVERY_MODE PRE_TEST)
