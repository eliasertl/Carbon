# Example applications. Every example accepts
#   --screenshot <file.png>   render one settled frame offscreen, save it and exit
#   --theme light|dark
#   --scale <factor>
add_subdirectory(Common)
add_subdirectory(MinimalIntegration)
add_subdirectory(CustomComponent)
# The Gallery shows the extension components too, so it needs CarbonExtensions.
if(CARBON_BUILD_EXTENSIONS)
    add_subdirectory(Gallery)
else()
    message(STATUS "Carbon: skipping the Gallery example because CARBON_BUILD_EXTENSIONS is OFF")
endif()
