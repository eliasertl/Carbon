# The web app published on GitHub Pages: a start screen that leads to the Gallery and to a reader for the
# documentation, built only with Emscripten, as Examples/Web/index.html (with index.js, index.wasm and index.data).
# Docs/Building.md, "The web app", explains how to build and deploy it.
set(CARBON_WEBAPP_REPOSITORY_URL "https://github.com/eliasertl/Carbon" CACHE STRING
    "Web address of the repository that the web app links to")
set(CARBON_WEBAPP_SOURCE_REF "main" CACHE STRING
    "Branch, tag or commit whose files the web app's links to source files open")

add_executable(WebApp
    DocImageCache.cpp
    DocImageCache.h
    DocLibrary.cpp
    DocLibrary.h
    DocsReader.cpp
    DocsReader.h
    Main.cpp
    Markdown.cpp
    Markdown.h
    MarkdownView.cpp
    MarkdownView.h
    StartScreen.cpp
    StartScreen.h
    StbImage.h
    WebPlatform.cpp
    WebPlatform.h
)
target_link_libraries(WebApp PRIVATE CarbonExampleOpenGLES CarbonGallery)
target_compile_definitions(WebApp PRIVATE
    "CARBON_WEBAPP_REPOSITORY_URL=\"${CARBON_WEBAPP_REPOSITORY_URL}\""
    "CARBON_WEBAPP_SOURCE_REF=\"${CARBON_WEBAPP_SOURCE_REF}\""
)
carbon_configure_example(WebApp Web index)

# stb is third-party code: compiled separately, without Carbon's warning flags. It decodes the documentation's PNGs.
add_library(CarbonWebAppStb OBJECT StbImage.cpp)
target_link_libraries(CarbonWebAppStb PRIVATE ${CARBON_DEPS_STB_NAME})
target_sources(WebApp PRIVATE $<TARGET_OBJECTS:CarbonWebAppStb>)

# The Docs folder is packed into index.data at link time and appears as /Docs in the page's file system. Every
# file of it is a link dependency, and the list is gathered again on every build, so a new or changed document is
# in the next build without touching any code.
set(docs "${PROJECT_SOURCE_DIR}/Docs")
file(GLOB_RECURSE docs_files CONFIGURE_DEPENDS "${docs}/*")
target_link_options(WebApp PRIVATE "--preload-file=${docs}@/Docs")
set_property(TARGET WebApp APPEND PROPERTY LINK_DEPENDS ${docs_files})
