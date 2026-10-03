include(CMake/CarbonTarget.cmake)

add_library(Carbon STATIC)
add_library(Carbon::Carbon ALIAS Carbon)

# Public headers: the supported API surface. Only these are installed, and only these may be included by
# CarbonExtensions and by custom components.
set(CARBON_PUBLIC_HEADERS
    src/Carbon/Carbon.h
    src/Carbon/Core/Assert.h
    src/Carbon/Core/Callbacks.h
    src/Carbon/Core/Color.h
    src/Carbon/Core/ContentScale.h
    src/Carbon/Core/Context.h
    src/Carbon/Core/ContextDescription.h
    src/Carbon/Core/EdgeInsets.h
    src/Carbon/Core/ID.h
    src/Carbon/Core/Log.h
    src/Carbon/Core/Math.h
    src/Carbon/Core/Platform.h
    src/Carbon/Core/Rect.h
    src/Carbon/Core/UTF8.h
    src/Carbon/Core/Vec2.h
    src/Carbon/Core/Version.h
    src/Carbon/Draw/DrawList.h
    src/Carbon/Draw/DrawTypes.h
    src/Carbon/Input/Cursor.h
    src/Carbon/Input/IO.h
    src/Carbon/Input/Input.h
    src/Carbon/Input/InputEvent.h
    src/Carbon/Input/Key.h
    src/Carbon/Input/MouseButton.h
)

# Internal headers: implementation details, never installed.
set(CARBON_INTERNAL_HEADERS
    src/Carbon/Core/ContextInternal.h
    src/Carbon/Input/InputStateInternal.h
)

set(CARBON_SOURCES
    src/Carbon/Core/Assert.cpp
    src/Carbon/Core/Context.cpp
    src/Carbon/Core/ID.cpp
    src/Carbon/Core/Log.cpp
    src/Carbon/Core/UTF8.cpp
    src/Carbon/Core/Version.cpp
    src/Carbon/Draw/DrawList.cpp
    src/Carbon/Input/IO.cpp
    src/Carbon/Input/Input.cpp
    src/Carbon/Input/InputStateInternal.cpp
)

target_sources(Carbon
    PRIVATE ${CARBON_SOURCES} ${CARBON_INTERNAL_HEADERS}
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES ${CARBON_PUBLIC_HEADERS}
)

target_compile_definitions(Carbon PRIVATE
    CARBON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CARBON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CARBON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(CARBON_FORCE_ASSERTS)
    target_compile_definitions(Carbon PUBLIC CB_FORCE_ASSERTS=1)
endif()

# Dawn types appear in Carbon's public headers, so it is a public dependency. FreeType and
# HarfBuzz are implementation details; as static libraries they still reach the final link.
target_link_libraries(Carbon
    PUBLIC ${CARBON_DEPS_DAWN_NAME}
    PRIVATE ${CARBON_DEPS_FREETYPE_NAME} ${CARBON_DEPS_HARFBUZZ_NAME}
)

carbon_configure_target(Carbon)
