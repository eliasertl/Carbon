# A window without the system's title bar: Carbon draws the header, the example moves and resizes the window.
add_executable(CustomTitleBar
    Main.cpp
    TitleBar.cpp
    TitleBar.h
    WindowControls.cpp
    WindowControls.h
)
target_link_libraries(CustomTitleBar PRIVATE CarbonExampleCommon Carbon::Extensions ${CARBON_DEPS_GLFW_NAME})
if(WIN32)
    target_link_libraries(CustomTitleBar PRIVATE dwmapi) # rounded corners on Windows 11
endif()
carbon_configure_target(CustomTitleBar)
carbon_copy_dawn_runtime(CustomTitleBar)
