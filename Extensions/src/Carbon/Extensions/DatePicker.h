#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Carbon/Extension.h"
#include "Carbon/Extensions/DateTime.h"

namespace Carbon
{
    /// What a date picker shows and edits.
    enum class DatePickerElements : uint8_t
    {
        Date,
        Time,
        DateAndTime
    };

    /// The order of year, month and day in a date.
    enum class DateOrder : uint8_t
    {
        YearMonthDay,
        DayMonthYear,
        MonthDayYear
    };

    /// How a date picker writes dates and times. Start from a preset and change what differs.
    struct DateFormat
    {
        DateOrder Order = DateOrder::YearMonthDay;
        /// Between year, month and day.
        char DateSeparator = '-';
        /// Two digits for days and months below 10 ("05" rather than "5").
        bool PadsDay = true;
        bool PadsMonth = true;
        /// Between the date and the time.
        std::string_view DateTimeSeparator = " ";
        /// 24-hour time; otherwise 12-hour time with AM and PM.
        bool Uses24HourClock = true;
        /// Two digits for hours below 10.
        bool PadsHour = true;
        /// Between hour and minute.
        char TimeSeparator = ':';

        /// ISO 8601: 2026-10-05 14:30.
        static DateFormat ISO() { return {}; }
        /// German: 05.10.2026, 14:30.
        static DateFormat German()
        {
            return {.Order = DateOrder::DayMonthYear, .DateSeparator = '.', .DateTimeSeparator = ", "};
        }
        /// United States: 10/5/2026, 2:30 PM.
        static DateFormat US()
        {
            return {.Order = DateOrder::MonthDayYear,
                    .DateSeparator = '/',
                    .PadsDay = false,
                    .PadsMonth = false,
                    .DateTimeSeparator = ", ",
                    .Uses24HourClock = false,
                    .PadsHour = false};
        }
    };

    /// Per-call options of DatePicker. All fields are optional.
    struct DatePickerOptions
    {
        DatePickerElements Elements = DatePickerElements::Date;
        DateFormat Format = DateFormat::ISO();
        /// The earliest and latest values that can be entered.
        std::optional<DateTime> MinDate = {};
        std::optional<DateTime> MaxDate = {};
        /// Steps of the minute: 1, or a divisor of 60 such as 5 or 15.
        int MinuteInterval = 1;
        /// A stepper next to the field that changes the selected element, as in macOS's textual date picker.
        bool ShowsStepper = true;
        /// The first column of the calendar.
        Weekday FirstWeekday = Weekday::Monday;
        /// The day the calendar marks as today; the computer's local date when not set.
        std::optional<DateTime> Today = {};
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
    };

    /// The textual date picker: a field that shows a date, a time or both, in which each element (year, month,
    /// day, hour, minute, AM/PM) is edited on its own. Click an element or move to it with the left and right
    /// arrow keys, then type a number or step it with the up and down arrow keys or the stepper. Clicking the date
    /// opens a calendar below the field. Returns true on frames the value changed.
    ///
    /// The label identifies the picker and is not drawn.
    bool DatePicker(std::string_view label, DateTime* value, const DatePickerOptions& options = {});

    /// Writes a value as a date picker with these elements and this format shows it, and returns the text (a view
    /// of `buffer`). 32 characters are always enough.
    std::string_view FormatDateTime(const DateTime& value, DatePickerElements elements, const DateFormat& format,
                                    std::span<char> buffer);
} // namespace Carbon
