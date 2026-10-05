add_executable(Gallery
    CorePages.cpp
    ExtensionPages.cpp
    GalleryState.h
    Main.cpp
    Pages.h
)
target_link_libraries(Gallery PRIVATE CarbonExampleCommon Carbon::Extensions)
# The Reflection page needs CarbonReflection.
if(TARGET CarbonReflection)
    target_sources(Gallery PRIVATE ReflectionPage.cpp)
    target_link_libraries(Gallery PRIVATE Carbon::Reflection)
    target_compile_definitions(Gallery PRIVATE CARBON_GALLERY_HAVE_REFLECTION=1)
endif()
carbon_configure_target(Gallery)
carbon_copy_dawn_runtime(Gallery)
