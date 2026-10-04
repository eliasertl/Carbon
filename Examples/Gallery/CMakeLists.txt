add_executable(Gallery
    CorePages.cpp
    ExtensionPages.cpp
    GalleryState.h
    Main.cpp
    MorePages.cpp
    Pages.h
)
target_link_libraries(Gallery PRIVATE CarbonExampleCommon Carbon::Extensions)
carbon_configure_target(Gallery)
carbon_copy_dawn_runtime(Gallery)
