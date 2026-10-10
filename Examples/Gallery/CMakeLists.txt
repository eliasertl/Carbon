# The Gallery's pages and its shell (sidebar, header) are the library CarbonGallery, which the Gallery example and
# the web app (Examples/WebApp) share. The example itself is only its entry point, Main.cpp.
add_library(CarbonGallery STATIC
    CorePages.cpp
    DragDropPage.cpp
    ExtensionPages.cpp
    Gallery.cpp
    Gallery.h
    GalleryState.h
    Pages.h
)
target_include_directories(CarbonGallery PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(CarbonGallery PUBLIC CarbonExampleBase Carbon::Extensions)
# The Reflection page needs CarbonReflection.
if(TARGET CarbonReflection)
    target_sources(CarbonGallery PRIVATE ReflectionPage.cpp)
    target_link_libraries(CarbonGallery PUBLIC Carbon::Reflection)
    target_compile_definitions(CarbonGallery PUBLIC CARBON_GALLERY_HAVE_REFLECTION=1)
endif()
carbon_configure_target(CarbonGallery)

carbon_add_backend_examples(Gallery
    SOURCES Main.cpp
    LIBRARIES CarbonGallery
)
