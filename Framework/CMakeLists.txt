include(CMake/CarbonTarget.cmake)

add_library(Carbon STATIC)
add_library(Carbon::Carbon ALIAS Carbon)

# Public headers: the supported API surface. Only these are installed.
set(CARBON_PUBLIC_HEADERS
    src/Carbon/Carbon.h
    src/Carbon/Core/Version.h
)

set(CARBON_SOURCES
    src/Carbon/Core/Version.cpp
)

target_sources(Carbon
    PRIVATE ${CARBON_SOURCES}
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES ${CARBON_PUBLIC_HEADERS}
)

target_compile_definitions(Carbon PRIVATE
    CARBON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CARBON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CARBON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Dawn types appear in Carbon's public headers, so it is a public dependency. FreeType and
# HarfBuzz are implementation details; as static libraries they still reach the final link.
target_link_libraries(Carbon
    PUBLIC ${CARBON_DEPS_DAWN_NAME}
    PRIVATE ${CARBON_DEPS_FREETYPE_NAME} ${CARBON_DEPS_HARFBUZZ_NAME}
)

carbon_configure_target(Carbon)
