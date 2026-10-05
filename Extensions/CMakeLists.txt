add_library(CarbonExtensions STATIC)
add_library(Carbon::Extensions ALIAS CarbonExtensions)

# Public headers: one per component, plus the umbrella header.
set(CARBON_EXTENSIONS_PUBLIC_HEADERS
    src/Carbon/Extensions/Alert.h
    src/Carbon/Extensions/Chart.h
    src/Carbon/Extensions/ColorWell.h
    src/Carbon/Extensions/ColumnView.h
    src/Carbon/Extensions/ComboBox.h
    src/Carbon/Extensions/DatePicker.h
    src/Carbon/Extensions/DatePickerCalendar.h
    src/Carbon/Extensions/DateTime.h
    src/Carbon/Extensions/List.h
    src/Carbon/Extensions/Menu.h
    src/Carbon/Extensions/MenuBar.h
    src/Carbon/Extensions/Notification.h
    src/Carbon/Extensions/OutlineView.h
    src/Carbon/Extensions/PathControl.h
    src/Carbon/Extensions/Popover.h
    src/Carbon/Extensions/PopUpButton.h
    src/Carbon/Extensions/ProgressIndicator.h
    src/Carbon/Extensions/PullDownButton.h
    src/Carbon/Extensions/RadioGroup.h
    src/Carbon/Extensions/SearchField.h
    src/Carbon/Extensions/SegmentedControl.h
    src/Carbon/Extensions/Sheet.h
    src/Carbon/Extensions/Sidebar.h
    src/Carbon/Extensions/SplitView.h
    src/Carbon/Extensions/Stepper.h
    src/Carbon/Extensions/Table.h
    src/Carbon/Extensions/TabView.h
    src/Carbon/Extensions/TokenField.h
    src/Carbon/Extensions/Toolbar.h
    src/Carbon/Extensions/Extensions.h
)

# Helpers shared by several components; not installed.
set(CARBON_EXTENSIONS_INTERNAL_HEADERS
    src/Carbon/Extensions/Internal/ColumnLayout.h
    src/Carbon/Extensions/Internal/MenuInternal.h
    src/Carbon/Extensions/Internal/SelectionList.h
)

set(CARBON_EXTENSIONS_SOURCES
    src/Carbon/Extensions/Alert.cpp
    src/Carbon/Extensions/Chart.cpp
    src/Carbon/Extensions/ColorWell.cpp
    src/Carbon/Extensions/ColumnView.cpp
    src/Carbon/Extensions/ComboBox.cpp
    src/Carbon/Extensions/DatePicker.cpp
    src/Carbon/Extensions/DatePickerCalendar.cpp
    src/Carbon/Extensions/DateTime.cpp
    src/Carbon/Extensions/List.cpp
    src/Carbon/Extensions/Menu.cpp
    src/Carbon/Extensions/MenuBar.cpp
    src/Carbon/Extensions/Notification.cpp
    src/Carbon/Extensions/OutlineView.cpp
    src/Carbon/Extensions/PathControl.cpp
    src/Carbon/Extensions/Popover.cpp
    src/Carbon/Extensions/PopUpButton.cpp
    src/Carbon/Extensions/ProgressIndicator.cpp
    src/Carbon/Extensions/PullDownButton.cpp
    src/Carbon/Extensions/RadioGroup.cpp
    src/Carbon/Extensions/SearchField.cpp
    src/Carbon/Extensions/SegmentedControl.cpp
    src/Carbon/Extensions/Sheet.cpp
    src/Carbon/Extensions/Sidebar.cpp
    src/Carbon/Extensions/SplitView.cpp
    src/Carbon/Extensions/Stepper.cpp
    src/Carbon/Extensions/Table.cpp
    src/Carbon/Extensions/TabView.cpp
    src/Carbon/Extensions/TokenField.cpp
    src/Carbon/Extensions/Toolbar.cpp
    src/Carbon/Extensions/Extensions.cpp
    src/Carbon/Extensions/Internal/ColumnLayout.cpp
    src/Carbon/Extensions/Internal/SelectionList.cpp
)

target_sources(CarbonExtensions
    PRIVATE ${CARBON_EXTENSIONS_SOURCES} ${CARBON_EXTENSIONS_INTERNAL_HEADERS}
    PUBLIC FILE_SET HEADERS BASE_DIRS src FILES ${CARBON_EXTENSIONS_PUBLIC_HEADERS}
)

# Extensions are built only on Carbon's public API; they never link Carbon's private dependencies.
target_link_libraries(CarbonExtensions PUBLIC Carbon::Carbon)

carbon_configure_target(CarbonExtensions)
# The installed package exports the target under the same name as the alias: Carbon::Extensions.
set_target_properties(CarbonExtensions PROPERTIES EXPORT_NAME Extensions)

if(CARBON_INSTALL)
    include(GNUInstallDirs)
    install(TARGETS CarbonExtensions EXPORT CarbonExtensionsTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
    install(EXPORT CarbonExtensionsTargets NAMESPACE Carbon:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Carbon")
endif()
