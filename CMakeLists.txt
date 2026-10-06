cmake_minimum_required(VERSION 3.25)

project(Carbon VERSION 0.1.0 DESCRIPTION "Immediate-mode C++ UI framework with a macOS look" LANGUAGES C CXX)

option(CARBON_BUILD_EXTENSIONS "Build the CarbonExtensions library" ON)
option(CARBON_BUILD_REFLECTION "Build the CarbonReflection library (needs CARBON_BUILD_EXTENSIONS)" ON)
option(CARBON_BUILD_EXAMPLES "Build the Carbon examples" ${PROJECT_IS_TOP_LEVEL})
option(CARBON_BUILD_TESTS "Build the Carbon unit tests" ${PROJECT_IS_TOP_LEVEL})
option(CARBON_BUILD_BENCHMARKS "Build the Carbon benchmarks (needs CARBON_BUILD_EXTENSIONS)" ${PROJECT_IS_TOP_LEVEL})
option(CARBON_INSTALL "Generate install rules and the CarbonConfig.cmake package" ${PROJECT_IS_TOP_LEVEL})
option(CARBON_WARNINGS_AS_ERRORS "Treat warnings in Carbon targets as errors (used by CI)" OFF)
option(CARBON_FORCE_ASSERTS "Keep CB_ASSERT checks active in optimized builds" OFF)

add_subdirectory(ThirdParty)
add_subdirectory(Framework)

if(CARBON_BUILD_EXTENSIONS)
    add_subdirectory(Extensions)
endif()

if(CARBON_BUILD_REFLECTION AND CARBON_BUILD_EXTENSIONS)
    add_subdirectory(Reflection)
elseif(CARBON_BUILD_REFLECTION)
    message(STATUS "Carbon: skipping CarbonReflection because CARBON_BUILD_EXTENSIONS is OFF")
endif()

if(CARBON_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif()

if(CARBON_BUILD_TESTS)
    enable_testing() # must run in the root directory so `ctest --test-dir <build>` finds the tests
    add_subdirectory(Tests)
endif()

if(CARBON_BUILD_BENCHMARKS)
    add_subdirectory(Benchmarks)
endif()
