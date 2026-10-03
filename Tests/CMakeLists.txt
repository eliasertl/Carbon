include(GoogleTest)

add_executable(CarbonTests
    src/Core/ContextTests.cpp
    src/Core/IDTests.cpp
    src/Core/MathTests.cpp
    src/Core/UTF8Tests.cpp
    src/Core/VersionTests.cpp
    src/Draw/DrawListTests.cpp
    src/Input/InputTests.cpp
    src/Support/ContextTest.h
)

target_include_directories(CarbonTests PRIVATE src)
target_link_libraries(CarbonTests PRIVATE Carbon::Carbon ${CARBON_DEPS_GOOGLETEST_NAME})

if(CARBON_BUILD_EXTENSIONS)
    target_link_libraries(CarbonTests PRIVATE Carbon::Extensions)
    target_compile_definitions(CarbonTests PRIVATE CARBON_TESTS_HAVE_EXTENSIONS=1)
endif()

carbon_configure_target(CarbonTests)

# PRE_TEST discovery keeps the build independent of being able to run the test binary.
gtest_discover_tests(CarbonTests DISCOVERY_MODE PRE_TEST)
