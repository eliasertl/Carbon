#pragma once

/// Umbrella header for the CarbonExtensions component library: Carbon itself plus every extension component.

#include "Carbon/Carbon.h"
#include "Carbon/Extensions/Alert.h"
#include "Carbon/Extensions/Chart.h"
#include "Carbon/Extensions/ColorWell.h"
#include "Carbon/Extensions/ColumnView.h"
#include "Carbon/Extensions/ComboBox.h"
#include "Carbon/Extensions/DatePicker.h"
#include "Carbon/Extensions/DatePickerCalendar.h"
#include "Carbon/Extensions/DateTime.h"
#include "Carbon/Extensions/List.h"
#include "Carbon/Extensions/Menu.h"
#include "Carbon/Extensions/MenuBar.h"
#include "Carbon/Extensions/Notification.h"
#include "Carbon/Extensions/OutlineView.h"
#include "Carbon/Extensions/PathControl.h"
#include "Carbon/Extensions/PopUpButton.h"
#include "Carbon/Extensions/Popover.h"
#include "Carbon/Extensions/ProgressIndicator.h"
#include "Carbon/Extensions/PullDownButton.h"
#include "Carbon/Extensions/RadioGroup.h"
#include "Carbon/Extensions/RowRange.h"
#include "Carbon/Extensions/SearchField.h"
#include "Carbon/Extensions/SegmentedControl.h"
#include "Carbon/Extensions/Sheet.h"
#include "Carbon/Extensions/Sidebar.h"
#include "Carbon/Extensions/SplitView.h"
#include "Carbon/Extensions/Stepper.h"
#include "Carbon/Extensions/TabView.h"
#include "Carbon/Extensions/Table.h"
#include "Carbon/Extensions/TokenField.h"
#include "Carbon/Extensions/Toolbar.h"

namespace Carbon
{
    /// Returns the version of the Carbon library that CarbonExtensions was built against.
    Version GetExtensionsVersion();
} // namespace Carbon