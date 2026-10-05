# A separate project that uses an *installed* Carbon, the way an application would:
#
#   cmake --install Build --prefix Install
#   cmake -S Tests/Package -B Build/Package "-DCMAKE_PREFIX_PATH=<path>/Install;<dawn-install>"
#   cmake --build Build/Package
#   ctest --test-dir Build/Package
#
# It proves two things that the main build cannot: that the package configuration works, and that custom
# components, CarbonExtensions and CarbonReflection compile with nothing but the installed (public) headers.
cmake_minimum_required(VERSION 3.25)

project(CarbonPackageCheck LANGUAGES C CXX)

find_package(Carbon CONFIG REQUIRED COMPONENTS Extensions Reflection)

set(CARBON_REPOSITORY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# An application with a custom component: the star rating of Examples/CustomComponent.
add_executable(PackageCheck
    Main.cpp
    "${CARBON_REPOSITORY_DIR}/Examples/CustomComponent/StarRating.cpp"
)
target_include_directories(PackageCheck PRIVATE "${CARBON_REPOSITORY_DIR}/Examples/CustomComponent")
target_link_libraries(PackageCheck PRIVATE Carbon::Carbon Carbon::Extensions Carbon::Reflection)
carbon_copy_dawn_runtime(PackageCheck)

# The sources of CarbonExtensions, compiled against the installed Carbon headers. Carbon's source tree is not on
# the include path, so an include of an internal header fails here.
file(GLOB_RECURSE extension_sources "${CARBON_REPOSITORY_DIR}/Extensions/src/*.cpp")
add_library(ExtensionsFromInstalledHeaders STATIC ${extension_sources})
target_include_directories(ExtensionsFromInstalledHeaders PRIVATE "${CARBON_REPOSITORY_DIR}/Extensions/src")
target_link_libraries(ExtensionsFromInstalledHeaders PRIVATE Carbon::Carbon)

# The same for the sources of CarbonReflection, against the installed Carbon and CarbonExtensions headers.
file(GLOB_RECURSE reflection_sources "${CARBON_REPOSITORY_DIR}/Reflection/src/*.cpp")
add_library(ReflectionFromInstalledHeaders STATIC ${reflection_sources})
target_include_directories(ReflectionFromInstalledHeaders PRIVATE "${CARBON_REPOSITORY_DIR}/Reflection/src")
target_link_libraries(ReflectionFromInstalledHeaders PRIVATE Carbon::Carbon Carbon::Extensions)

enable_testing()
add_test(NAME PackageCheck COMMAND PackageCheck)
