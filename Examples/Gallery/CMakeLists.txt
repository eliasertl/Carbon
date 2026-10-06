set(gallery_sources
    CorePages.cpp
    ExtensionPages.cpp
    GalleryState.h
    Main.cpp
    Pages.h
)
set(gallery_libraries Carbon::Extensions)
set(gallery_definitions)
# The Reflection page needs CarbonReflection.
if(TARGET CarbonReflection)
    list(APPEND gallery_sources ReflectionPage.cpp)
    list(APPEND gallery_libraries Carbon::Reflection)
    list(APPEND gallery_definitions CARBON_GALLERY_HAVE_REFLECTION=1)
endif()
carbon_add_backend_examples(Gallery
    SOURCES ${gallery_sources}
    LIBRARIES ${gallery_libraries}
    DEFINITIONS ${gallery_definitions}
)
