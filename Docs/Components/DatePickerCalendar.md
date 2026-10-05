# DatePickerCalendar

The graphical date picker: a month as a grid of days, for browsing and picking a date.
HIG: [Pickers](https://developer.apple.com/design/human-interface-guidelines/pickers) (date pickers, macOS)
Library: CarbonExtensions, `#include <Carbon/Extensions/DatePickerCalendar.h>`

```cpp
Carbon::DateTime due = { .Year = 2026, .Month = 10, .Day = 5 };
if (Carbon::DatePickerCalendar("Due date", &due, { .MinDate = Carbon::GetCurrentDateTime() }))
    Reschedule(due);
```

The function returns `true` on the frame the date changed. The label identifies the calendar and is not drawn.
The time of day in the value is kept: picking a day changes only the year, month and day. To edit a date in a
compact field that opens this calendar, use [DatePicker](DatePicker.md).

## Dates

`Carbon/Extensions/DateTime.h`, shared by both date components, declares the value:

```cpp
struct DateTime { int Year = 2000; int Month = 1; int Day = 1; int Hour = 0; int Minute = 0; };
```

It is local wall time as the application keeps it, in the Gregorian calendar. There are no time zones and Carbon
never converts the value; `GetCurrentDateTime()` reads the computer's clock in its local time. Values compare
chronologically with `<`, `==` and `<=>`. The helpers do calendar arithmetic through `std::chrono`:

| Function | Meaning |
| --- | --- |
| `IsValidDate`, `MakeValidDate` | Whether every field is in range; the nearest valid value |
| `GetDaysInMonth`, `IsLeapYear`, `GetWeekday` | Facts about the calendar |
| `AddDays`, `AddMonths` | Move a value; a day that does not exist in the new month becomes its last |
| `IsSameDay`, `ClampDate` | Compare by day; clamp into an optional minimum and maximum |
| `GetMonthName`, `GetWeekdayName` | English names |

## Options

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `FirstWeekday` | `Weekday` | `Monday` | The first column of the grid |
| `MinDate`, `MaxDate` | `DateTime` | none | The range of selectable values; days outside are dimmed and cannot be picked |
| `Today` | `DateTime` | the computer's date | The day marked as today |
| `Disabled` | `bool` | `false` | Dimmed, ignores input |

## Behaviour

- The header shows the month and year, with buttons for the previous month, today's month and the next month at
  its trailing edge. They change the month on display, not the value. A button that would lead only to days out
  of range is disabled.
- The grid always has six weeks, so the calendar keeps its height from month to month. Days of the neighbouring
  months are shown dimmed; picking one selects it and turns to its month.
- Today's number is drawn in the accent color, the selected day on an accent circle.
- The month on display follows the value whenever the value changes, from a click, a key or the application.
- A value that is invalid or outside the range is corrected and written back without reporting a change.

## Keyboard

| Key | Effect |
| --- | --- |
| Tab / Shift+Tab | Focus the grid; the calendar is one stop |
| Left / Right arrow | Previous / next day |
| Up / Down arrow | Same day of the previous / next week |
| Page Up / Page Down | Same day of the previous / next month; with Shift, of the previous / next year |
| Home / End | First / last day of the month |

Every move is clamped to `MinDate` and `MaxDate`. The month buttons are not Tab stops: the Page keys do the same.

## Guidance from the HIG

- Use the graphical style when people benefit from browsing days in a calendar; when space is tight and people
  know the date they want, use the textual style ([DatePicker](DatePicker.md)).
- Show the picker in context, next to what it edits, or in a popover; do not switch views to show it.
- Month and weekday names are English in Carbon; there is no localization.
