include(CMake/CarbonTarget.cmake)
include(CMake/CarbonAssets.cmake)
include(CMake/CarbonDawnRuntime.cmake)

add_library(Carbon STATIC)
add_library(Carbon::Carbon ALIAS Carbon)

# Files generated at build time (embedded assets, icon constants) live here, never in the source tree.
set(CARBON_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")

# Public headers: the supported API surface. Only these are installed, and only these may be included by
# CarbonExtensions and by custom components.
set(CARBON_PUBLIC_HEADERS
    src/Carbon/Carbon.h
    src/Carbon/Extension.h
    src/Carbon/Animation/Animation.h
    src/Carbon/Animation/Easing.h
    src/Carbon/Animation/Spring.h
    src/Carbon/Assets/EmbeddedAssets.h
    src/Carbon/Core/Assert.h
    src/Carbon/Core/Callbacks.h
    src/Carbon/Core/Color.h
    src/Carbon/Core/ContentScale.h
    src/Carbon/Core/Context.h
    src/Carbon/Core/ContextDescription.h
    src/Carbon/Core/EdgeInsets.h
    src/Carbon/Core/Hash.h
    src/Carbon/Core/ID.h
    src/Carbon/Core/Log.h
    src/Carbon/Core/Math.h
    src/Carbon/Core/Platform.h
    src/Carbon/Core/Rect.h
    src/Carbon/Core/State.h
    src/Carbon/Core/UTF8.h
    src/Carbon/Core/Vec2.h
    src/Carbon/Core/Version.h
    src/Carbon/Draw/DrawList.h
    src/Carbon/Draw/DrawTypes.h
    src/Carbon/Draw/Squircle.h
    src/Carbon/Input/Cursor.h
    src/Carbon/Input/IO.h
    src/Carbon/Input/Input.h
    src/Carbon/Input/InputEvent.h
    src/Carbon/Input/Key.h
    src/Carbon/Input/MouseButton.h
    src/Carbon/Interaction/Interaction.h
    src/Carbon/Layout/Grid.h
    src/Carbon/Layout/Layout.h
    src/Carbon/Layout/ScrollView.h
    src/Carbon/Layout/Size.h
    src/Carbon/Layout/Stack.h
    src/Carbon/Overlay/Overlay.h
    src/Carbon/Renderer/RendererBackend.h
    src/Carbon/Renderer/TextureFormat.h
    src/Carbon/Style/Style.h
    src/Carbon/Style/StyleColor.h
    src/Carbon/Style/StyleVar.h
    src/Carbon/Style/Theme.h
    src/Carbon/Text/Font.h
    src/Carbon/Text/TextSpec.h
    src/Carbon/Text/TextStyle.h
    src/Carbon/Widgets/Button.h
    src/Carbon/Widgets/ControlFeedback.h
    src/Carbon/Widgets/ControlSize.h
    src/Carbon/Widgets/Image.h
    src/Carbon/Widgets/Separator.h
    src/Carbon/Widgets/Slider.h
    src/Carbon/Widgets/Text.h
    src/Carbon/Widgets/TextField.h
    src/Carbon/Widgets/Toggle.h
    src/Carbon/Widgets/Tooltip.h
)

# Public headers generated at build time.
set(CARBON_GENERATED_PUBLIC_HEADERS
    "${CARBON_GENERATED_DIR}/Carbon/Text/Icons.h"
)

# Internal headers: implementation details, never installed.
set(CARBON_INTERNAL_HEADERS
    src/Carbon/Core/ContextInternal.h
    src/Carbon/Core/StateStorageInternal.h
    src/Carbon/Input/InputStateInternal.h
    src/Carbon/Interaction/InteractionInternal.h
    src/Carbon/Layout/LayoutInternal.h
    src/Carbon/Overlay/OverlayInternal.h
    src/Carbon/Renderer/RenderStateInternal.h
    src/Carbon/Style/StyleInternal.h
    src/Carbon/Text/Internal/FontFace.h
    src/Carbon/Text/Internal/GlyphAtlas.h
    src/Carbon/Text/Internal/TextSystem.h
    src/Carbon/Widgets/Internal/TextEditor.h
)

set(CARBON_SOURCES
    src/Carbon/Animation/Animation.cpp
    src/Carbon/Animation/Easing.cpp
    src/Carbon/Animation/Spring.cpp
    src/Carbon/Assets/EmbeddedAssets.cpp
    src/Carbon/Core/Assert.cpp
    src/Carbon/Core/Context.cpp
    src/Carbon/Core/ID.cpp
    src/Carbon/Core/Log.cpp
    src/Carbon/Core/StateStorageInternal.cpp
    src/Carbon/Core/UTF8.cpp
    src/Carbon/Core/Version.cpp
    src/Carbon/Draw/DrawList.cpp
    src/Carbon/Draw/Squircle.cpp
    src/Carbon/Input/IO.cpp
    src/Carbon/Input/Input.cpp
    src/Carbon/Input/InputStateInternal.cpp
    src/Carbon/Interaction/Interaction.cpp
    src/Carbon/Layout/Grid.cpp
    src/Carbon/Layout/Layout.cpp
    src/Carbon/Layout/ScrollView.cpp
    src/Carbon/Layout/Stack.cpp
    src/Carbon/Overlay/Overlay.cpp
    src/Carbon/Renderer/RendererBackend.cpp
    src/Carbon/Renderer/TextureFormat.cpp
    src/Carbon/Style/Style.cpp
    src/Carbon/Style/Theme.cpp
    src/Carbon/Text/Font.cpp
    src/Carbon/Text/Internal/FontFace.cpp
    src/Carbon/Text/Internal/GlyphAtlas.cpp
    src/Carbon/Text/Internal/TextSystem.cpp
    src/Carbon/Text/TextSpec.cpp
    src/Carbon/Widgets/Button.cpp
    src/Carbon/Widgets/ControlFeedback.cpp
    src/Carbon/Widgets/ControlSize.cpp
    src/Carbon/Widgets/Image.cpp
    src/Carbon/Widgets/Internal/TextEditor.cpp
    src/Carbon/Widgets/Separator.cpp
    src/Carbon/Widgets/Slider.cpp
    src/Carbon/Widgets/Text.cpp
    src/Carbon/Widgets/TextField.cpp
    src/Carbon/Widgets/Toggle.cpp
    src/Carbon/Widgets/Tooltip.cpp
)

# Renderer backends: all of a backend's code lives in Backends/<Name>/ and is compiled only when the backend is
# enabled. Its public header, which includes the graphics API, is installed only then.
if(CARBON_BACKEND_WEBGPU)
    list(APPEND CARBON_PUBLIC_HEADERS src/Carbon/Backends/WebGPU/WebGPUBackend.h)
    list(APPEND CARBON_INTERNAL_HEADERS src/Carbon/Backends/WebGPU/WebGPURendererInternal.h)
    list(APPEND CARBON_SOURCES
        src/Carbon/Backends/WebGPU/WebGPUBackend.cpp
        src/Carbon/Backends/WebGPU/WebGPURendererInternal.cpp
    )
endif()
if(CARBON_BACKEND_OPENGL)
    list(APPEND CARBON_PUBLIC_HEADERS src/Carbon/Backends/OpenGL/OpenGLBackend.h)
    list(APPEND CARBON_INTERNAL_HEADERS
        src/Carbon/Backends/OpenGL/OpenGLFunctionsInternal.h
        src/Carbon/Backends/OpenGL/OpenGLRendererInternal.h
    )
    list(APPEND CARBON_SOURCES
        src/Carbon/Backends/OpenGL/OpenGLBackend.cpp
        src/Carbon/Backends/OpenGL/OpenGLFunctionsInternal.cpp
        src/Carbon/Backends/OpenGL/OpenGLRendererInternal.cpp
    )
endif()
if(CARBON_BACKEND_VULKAN)
    list(APPEND CARBON_PUBLIC_HEADERS src/Carbon/Backends/Vulkan/VulkanBackend.h)
    list(APPEND CARBON_INTERNAL_HEADERS
        src/Carbon/Backends/Vulkan/VulkanAllocatorInternal.h
        src/Carbon/Backends/Vulkan/VulkanRendererInternal.h
    )
    list(APPEND CARBON_SOURCES
        src/Carbon/Backends/Vulkan/VulkanAllocatorInternal.cpp
        src/Carbon/Backends/Vulkan/VulkanBackend.cpp
        src/Carbon/Backends/Vulkan/VulkanRendererInternal.cpp
    )
endif()

# The public headers as they are written in an include directive. The PublicApiBoundary tests use this list to
# verify that CarbonExtensions and the CustomComponent example stay on the public API.
set(carbon_public_include_names "Carbon/Text/Icons.h")
foreach(header IN LISTS CARBON_PUBLIC_HEADERS)
    string(REGEX REPLACE "^src/" "" header_name "${header}")
    list(APPEND carbon_public_include_names "${header_name}")
endforeach()
set_property(TARGET Carbon PROPERTY CARBON_PUBLIC_INCLUDE_NAMES "${carbon_public_include_names}")

carbon_generate_icons("${PROJECT_SOURCE_DIR}/ThirdParty/Phosphor/src/regular/style.css")

target_sources(Carbon
    PRIVATE ${CARBON_SOURCES} ${CARBON_INTERNAL_HEADERS}
    PUBLIC FILE_SET HEADERS
        BASE_DIRS src "${CARBON_GENERATED_DIR}"
        FILES ${CARBON_PUBLIC_HEADERS} ${CARBON_GENERATED_PUBLIC_HEADERS}
)

# Embedded assets: the fonts become byte arrays in generated .cpp files.
carbon_embed_asset(Carbon PublicSansRoman "${PROJECT_SOURCE_DIR}/ThirdParty/PublicSans/fonts/variable/PublicSans[wght].ttf")
carbon_embed_asset(Carbon PublicSansItalic "${PROJECT_SOURCE_DIR}/ThirdParty/PublicSans/fonts/variable/PublicSans-Italic[wght].ttf")
carbon_embed_asset(Carbon JetBrainsMonoRoman "${PROJECT_SOURCE_DIR}/ThirdParty/JetBrainsMono/fonts/variable/JetBrainsMono[wght].ttf")
carbon_embed_asset(Carbon JetBrainsMonoItalic "${PROJECT_SOURCE_DIR}/ThirdParty/JetBrainsMono/fonts/variable/JetBrainsMono-Italic[wght].ttf")
carbon_embed_asset(Carbon PhosphorRegular "${PROJECT_SOURCE_DIR}/ThirdParty/Phosphor/src/regular/Phosphor.ttf")
carbon_embed_asset(Carbon PhosphorBold "${PROJECT_SOURCE_DIR}/ThirdParty/Phosphor/src/bold/Phosphor-Bold.ttf")
carbon_embed_asset(Carbon PhosphorFill "${PROJECT_SOURCE_DIR}/ThirdParty/Phosphor/src/fill/Phosphor-Fill.ttf")

target_compile_definitions(Carbon PRIVATE
    CARBON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CARBON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CARBON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(CARBON_FORCE_ASSERTS)
    target_compile_definitions(Carbon PUBLIC CB_FORCE_ASSERTS=1)
endif()

# FreeType and HarfBuzz are implementation details; as static libraries they still reach the final link.
target_link_libraries(Carbon PRIVATE ${CARBON_DEPS_FREETYPE_NAME} ${CARBON_DEPS_HARFBUZZ_NAME})

# ---- Renderer backends ------------------------------------------------------------------------
# Every enabled backend is compiled into Carbon. Its graphics API appears in the backend's public header, so the
# API's library is a public dependency, and CARBON_HAS_BACKEND_<NAME> tells hosts and examples it is there.
set(CARBON_ENABLED_BACKENDS "")
if(CARBON_BACKEND_WEBGPU)
    list(APPEND CARBON_ENABLED_BACKENDS WebGPU)
    target_compile_definitions(Carbon PUBLIC CARBON_HAS_BACKEND_WEBGPU=1)
    target_link_libraries(Carbon PUBLIC ${CARBON_DEPS_DAWN_NAME})
    carbon_embed_asset(Carbon WebGPUShader "${CMAKE_CURRENT_SOURCE_DIR}/src/Carbon/Backends/WebGPU/Shaders/Carbon.wgsl")
endif()
if(CARBON_BACKEND_VULKAN)
    list(APPEND CARBON_ENABLED_BACKENDS Vulkan)
    target_compile_definitions(Carbon PUBLIC CARBON_HAS_BACKEND_VULKAN=1)
    target_link_libraries(Carbon PUBLIC Vulkan::Vulkan)
    # GLSL is compiled to SPIR-V at build time and included as a list of words.
    carbon_compile_spirv(Carbon "${CMAKE_CURRENT_SOURCE_DIR}/src/Carbon/Backends/Vulkan/Shaders/Carbon.vert"
        "${CARBON_GENERATED_DIR}/Carbon/Backends/Vulkan/Carbon.vert.inc")
    carbon_compile_spirv(Carbon "${CMAKE_CURRENT_SOURCE_DIR}/src/Carbon/Backends/Vulkan/Shaders/Carbon.frag"
        "${CARBON_GENERATED_DIR}/Carbon/Backends/Vulkan/Carbon.frag.inc")
endif()
if(CARBON_BACKEND_OPENGL)
    list(APPEND CARBON_ENABLED_BACKENDS OpenGL)
    target_compile_definitions(Carbon PUBLIC CARBON_HAS_BACKEND_OPENGL=1)
    # GLSL 3.30 source, embedded as text and compiled by the host's driver.
    carbon_embed_asset(Carbon OpenGLVertexShader "${CMAKE_CURRENT_SOURCE_DIR}/src/Carbon/Backends/OpenGL/Shaders/Carbon.vert")
    carbon_embed_asset(Carbon OpenGLFragmentShader "${CMAKE_CURRENT_SOURCE_DIR}/src/Carbon/Backends/OpenGL/Shaders/Carbon.frag")
endif()
if(CARBON_ENABLED_BACKENDS)
    list(JOIN CARBON_ENABLED_BACKENDS ", " carbon_backend_text)
else()
    set(carbon_backend_text "none (headless only)")
endif()
message(STATUS "Carbon: renderer backends: ${carbon_backend_text}")
set_property(TARGET Carbon PROPERTY CARBON_BACKENDS "${CARBON_ENABLED_BACKENDS}")

carbon_configure_target(Carbon)

# ---- Installation -----------------------------------------------------------------------------
# `cmake --install` produces a package that other projects use with find_package(Carbon CONFIG).
if(CARBON_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    set(CARBON_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Carbon")

    install(TARGETS Carbon EXPORT CarbonTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    # Carbon is a static library, so whoever links it also links FreeType and HarfBuzz. Copies built from the
    # submodules are installed next to Carbon and exported with it (as Carbon::freetype and Carbon::harfbuzz);
    # copies that were found on the system are found again by CarbonConfig.cmake.
    foreach(dependency IN ITEMS FREETYPE HARFBUZZ)
        get_target_property(dependency_target ${CARBON_DEPS_${dependency}_NAME} ALIASED_TARGET)
        if(NOT dependency_target)
            set(dependency_target ${CARBON_DEPS_${dependency}_NAME})
        endif()
        get_target_property(dependency_is_imported ${dependency_target} IMPORTED)
        if(NOT dependency_is_imported)
            install(TARGETS ${dependency_target} EXPORT CarbonTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
        endif()
    endforeach()

    install(EXPORT CarbonTargets NAMESPACE Carbon:: DESTINATION "${CARBON_INSTALL_CMAKEDIR}")

    get_property(CARBON_PACKAGE_DEPENDENCY_CODE GLOBAL PROPERTY CARBON_PACKAGE_DEPENDENCY_CODE)
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/CMake/CarbonConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/CarbonConfig.cmake"
        INSTALL_DESTINATION "${CARBON_INSTALL_CMAKEDIR}"
    )
    # Before 1.0 a new minor version may change the API.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/CarbonConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/CarbonConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/CarbonConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/CMake/CarbonDawnRuntime.cmake"
        DESTINATION "${CARBON_INSTALL_CMAKEDIR}"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/Carbon"
    )
    # The licenses of what is inside the installed libraries: the embedded fonts, and FreeType and HarfBuzz.
    # The fonts' licenses require their text to accompany every distribution.
    set(carbon_license_files
        "PublicSans/OFL.txt|PublicSans-OFL.txt"
        "JetBrainsMono/OFL.txt|JetBrainsMono-OFL.txt"
        "Phosphor/LICENSE|Phosphor-LICENSE.txt"
        "FreeType/docs/FTL.TXT|FreeType-FTL.txt"
        "HarfBuzz/COPYING|HarfBuzz-COPYING.txt"
    )
    foreach(license_file IN LISTS carbon_license_files)
        string(REPLACE "|" ";" license_file "${license_file}")
        list(GET license_file 0 license_source)
        list(GET license_file 1 license_name)
        if(EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/${license_source}")
            install(FILES "${PROJECT_SOURCE_DIR}/ThirdParty/${license_source}"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/Carbon/Licenses" RENAME "${license_name}"
            )
        endif()
    endforeach()
endif()
