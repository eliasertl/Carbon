add_executable(DX9Minimal Main.cpp)
target_link_libraries(DX9Minimal PRIVATE CarbonExampleBase d3d9)
carbon_configure_example(DX9Minimal DX9)
