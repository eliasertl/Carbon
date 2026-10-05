add_library(CarbonReflection STATIC)
add_library(Carbon::Reflection ALIAS CarbonReflection)

# Public headers: the API, the umbrella header, and the Detail/ headers that the public templates are made of.
# Detail/ is installed because templates are compiled in the application; its contents live in Carbon::Internal
# and are not meant to be used directly.
set(CARBON_REFLECTION_PUBLIC_HEADERS
    src/Carbon/Reflection/Enum.h
    src/Carbon/Reflection/Macros.h
    src/Carbon/Reflection/Reflect.h
    src/Carbon/Reflection/ReflectEnumOptions.h
    src/Carbon/Reflection/ReflectFieldOptions.h
    src/Carbon/Reflection/ReflectOptions.h
    src/Carbon/Reflection/Reflection.h
    src/Carbon/Reflection/Struct.h
    src/Carbon/Reflection/Detail/Description.h
    src/Carbon/Reflection/Detail/DisplayName.h
    src/Carbon/Reflection/Detail/EnumModel.h
    src/Carbon/Reflection/Detail/FieldTie.h
    src/Carbon/Reflection/Detail/ReflectWidgets.h
    src/Carbon/Reflection/Detail/Signature.h
    src/Carbon/Reflection/Detail/StructModel.h
)

set(CARBON_REFLECTION_SOURCES
    src/Carbon/Reflection/Reflect.cpp
    src/Carbon/Reflection/Detail/DisplayName.cpp
)

target_sources(CarbonReflection
    PRIVATE ${CARBON_REFLECTION_SOURCES}
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES ${CARBON_REFLECTION_PUBLIC_HEADERS}
)

# Reflection draws with the extension components and uses only the public APIs of Carbon and CarbonExtensions.
target_link_libraries(CarbonReflection PUBLIC Carbon::Extensions)

carbon_configure_target(CarbonReflection)
# The installed package exports the target under the same name as the alias: Carbon::Reflection.
set_target_properties(CarbonReflection PROPERTIES EXPORT_NAME Reflection)

if(CARBON_INSTALL)
    include(GNUInstallDirs)
    install(TARGETS CarbonReflection EXPORT CarbonReflectionTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(EXPORT CarbonReflectionTargets NAMESPACE Carbon:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Carbon")
endif()
