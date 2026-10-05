#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/DateTime.h"

namespace Carbon
{
    /// Per-call options of DatePickerCalendar. All fields are optional.
    struct DatePickerCalendarOptions
    {
        /// The first column of the month grid.
        Weekday FirstWeekday = Weekday::Monday;
        /// The earliest and latest selectable values; days outside are shown dimmed and cannot be picked.
        std::optional<DateTime> MinDate = {};
        std::optional<DateTime> MaxDate = {};
        /// The day marked as today; the computer's local date when not set.
        std::optional<DateTime> Today = {};
        bool Disabled = false;
    };

    /// The graphical date picker: a month as a grid of days, with buttons to the previous and next month. Click a
    /// day, or use the arrow keys while the grid has focus. The time of day of `value` is kept. Returns true on
    /// frames the value changed.
    ///
    /// The label identifies the calendar and is not drawn.
    bool DatePickerCalendar(std::string_view label, DateTime* value, const DatePickerCalendarOptions& options = {});
} // namespace Carbon
