# StarRating.h/.cpp are the custom component: they include Carbon/Extension.h and nothing else of Carbon, which
# the PublicApiBoundary test enforces.
add_executable(CustomComponent
    Main.cpp
    StarRating.cpp
    StarRating.h
)
target_link_libraries(CustomComponent PRIVATE CarbonExampleCommon)
carbon_configure_target(CustomComponent)
carbon_copy_dawn_runtime(CustomComponent)
