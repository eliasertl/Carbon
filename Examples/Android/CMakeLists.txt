# The native part of the Android Gallery, built by Gradle (build.gradle.kts) for each ABI: Carbon with its OpenGL
# ES backend, the Gallery's pages, and the host that connects them to GameActivity. Docs/Building.md, "Android".
cmake_minimum_required(VERSION 3.25)

project(CarbonAndroidGallery LANGUAGES C CXX)

set(carbon_root "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Carbon itself, with the OpenGL ES backend only: Android has no desktop OpenGL, and Vulkan and WebGPU are not used
# here. No examples, tests or benchmarks; those need GLFW and a desktop.
set(CARBON_BACKEND_OPENGLES ON CACHE BOOL "" FORCE)
set(CARBON_BACKEND_OPENGL OFF CACHE BOOL "" FORCE)
set(CARBON_BACKEND_VULKAN OFF CACHE BOOL "" FORCE)
set(CARBON_BACKEND_WEBGPU OFF CACHE BOOL "" FORCE)
set(CARBON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CARBON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CARBON_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(CARBON_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${carbon_root}" Carbon)

# GameActivity's native half, from the games-activity package through prefab.
find_package(game-activity REQUIRED CONFIG)

# The Gallery's pages and shell, the same sources as Examples/Gallery's CarbonGallery library. Only
# Examples/Common/GraphicsDevice.h comes from the desktop host, as the interface the Images page creates its
# texture through.
set(gallery "${carbon_root}/Examples/Gallery")
add_library(CarbonGallery STATIC
    "${gallery}/CorePages.cpp"
    "${gallery}/DragDropPage.cpp"
    "${gallery}/ExtensionPages.cpp"
    "${gallery}/Gallery.cpp"
)
target_include_directories(CarbonGallery PUBLIC "${gallery}" "${carbon_root}/Examples/Common")
target_link_libraries(CarbonGallery PUBLIC Carbon::Extensions)
if(TARGET CarbonReflection)
    target_sources(CarbonGallery PRIVATE "${gallery}/ReflectionPage.cpp")
    target_link_libraries(CarbonGallery PUBLIC Carbon::Reflection)
    target_compile_definitions(CarbonGallery PUBLIC CARBON_GALLERY_HAVE_REFLECTION=1)
endif()
carbon_configure_target(CarbonGallery)

# libGallery.so, the library the manifest names. GameActivity calls its android_main on a thread of its own.
add_library(Gallery SHARED
    AndroidDevice.cpp
    AndroidDevice.h
    AndroidHost.cpp
    AndroidHost.h
    AndroidMain.cpp
    AndroidText.cpp
    AndroidText.h
    "${carbon_root}/Examples/Common/SystemFonts.cpp"
)
target_link_libraries(Gallery PRIVATE CarbonGallery game-activity::game-activity_static EGL GLESv3 android log)
# The static GameActivity library is reached only from Java; keep the function the Java side calls first.
target_link_options(Gallery PRIVATE "-u" "Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")
carbon_configure_target(Gallery)
