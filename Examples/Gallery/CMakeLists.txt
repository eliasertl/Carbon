set(gallery_sources
    CorePages.cpp
    ExtensionPages.cpp
    GalleryState.h
    Main.cpp
    Pages.h
)
set(gallery_libraries Carbon::Extensions)
# The Reflection page needs CarbonReflection.
if(TARGET CarbonReflection)
    list(APPEND gallery_sources ReflectionPage.cpp)
    list(APPEND gallery_libraries Carbon::Reflection)
endif()
carbon_add_backend_examples(Gallery SOURCES ${gallery_sources} LIBRARIES ${gallery_libraries})
if(TARGET CarbonReflection)
    foreach(backend IN LISTS CARBON_EXAMPLE_BACKENDS)
        target_compile_definitions(${backend}Gallery PRIVATE CARBON_GALLERY_HAVE_REFLECTION=1)
    endforeach()
endif()
