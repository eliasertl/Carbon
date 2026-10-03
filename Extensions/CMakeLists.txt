add_library(CarbonExtensions STATIC)
add_library(Carbon::Extensions ALIAS CarbonExtensions)

set(CARBON_EXTENSIONS_PUBLIC_HEADERS
    src/Carbon/Extensions/Extensions.h
)

set(CARBON_EXTENSIONS_SOURCES
    src/Carbon/Extensions/Extensions.cpp
)

target_sources(CarbonExtensions
    PRIVATE ${CARBON_EXTENSIONS_SOURCES}
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES ${CARBON_EXTENSIONS_PUBLIC_HEADERS}
)

# Extensions are built only on Carbon's public API; they never link Carbon's private dependencies.
target_link_libraries(CarbonExtensions PUBLIC Carbon::Carbon)

carbon_configure_target(CarbonExtensions)
