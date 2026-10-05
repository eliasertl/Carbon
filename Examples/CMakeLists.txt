# Example applications. Every example accepts
#   --screenshot <file.png>   render one settled frame offscreen, save it and exit
#   --theme light|dark
#   --scale <factor>
add_subdirectory(Common)
add_subdirectory(MinimalIntegration)
add_subdirectory(CustomComponent)
# The Gallery and CustomTitleBar use the extension components, so they need CarbonExtensions.
if(CARBON_BUILD_EXTENSIONS)
    add_subdirectory(Gallery)
    add_subdirectory(CustomTitleBar)
else()
    message(STATUS "Carbon: skipping the Gallery and CustomTitleBar examples because CARBON_BUILD_EXTENSIONS is OFF")
endif()
# The Reflection example needs CarbonReflection.
if(TARGET CarbonReflection)
    add_subdirectory(Reflection)
else()
    message(STATUS "Carbon: skipping the Reflection example because CarbonReflection is not built")
endif()
