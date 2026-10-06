# A window without the system's title bar: Carbon draws the header, the example moves and resizes the window. A web
# page has no window of its own, so there is no browser build.
set(title_bar_libraries Carbon::Extensions ${CARBON_DEPS_GLFW_NAME})
if(WIN32)
    list(APPEND title_bar_libraries dwmapi) # rounded corners on Windows 11
endif()
carbon_add_backend_examples(CustomTitleBar
    SOURCES Main.cpp TitleBar.cpp TitleBar.h WindowControls.cpp WindowControls.h
    LIBRARIES ${title_bar_libraries}
    NO_WEB
)
