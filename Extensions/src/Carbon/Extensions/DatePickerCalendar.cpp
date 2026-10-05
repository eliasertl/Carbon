#include "Carbon/Extensions/DatePickerCalendar.h"

#include <algorithm>
#include <charconv>
#include <cstring>

namespace Carbon
{
    namespace
    {
        constexpr int Columns = 7;
        // Always six weeks, so that the calendar keeps its height from month to month.
        constexpr int Weeks = 6;
        constexpr int Cells = Columns * Weeks;
        constexpr float CellWidth = 32.0f;
        constexpr float CellHeight = 28.0f;
        constexpr float DayDiameter = 24.0f;
        constexpr float HeaderHeight = 28.0f;
        constexpr float HeaderGap = 4.0f;
        constexpr float WeekdayHeight = 20.0f;
        constexpr float NavigationButton = 22.0f;
        constexpr float TitleInset = 6.0f;

        // Remembered per calendar: the month on display. It follows the value whenever the value changes.
        struct CalendarState
        {
            int Year;
            int Month;
            DateTime ShownValue;
            bool HasMonth;
        };

        // The day of a value, without its time: days are enabled by day, not by the minute.
        DateTime GetDay(const DateTime& date)
        {
            DateTime day = date;
            day.Hour = 0;
            day.Minute = 0;
            return day;
        }

        bool IsDayInRange(const DateTime& date, const DatePickerCalendarOptions& options)
        {
            if (options.MinDate.has_value() && GetDay(date) < GetDay(*options.MinDate))
                return false;
            return !options.MaxDate.has_value() || GetDay(date) <= GetDay(*options.MaxDate);
        }

        // Writes the day of the month; returns the text.
        std::string_view FormatNumber(char (&buffer)[8], int number)
        {
            const char* end = std::to_chars(buffer, buffer + sizeof(buffer), number).ptr;
            return std::string_view(buffer, static_cast<size_t>(end - buffer));
        }
    } // namespace

    bool DatePickerCalendar(std::string_view label, DateTime* value, const DatePickerCalendarOptions& options)
    {
        CB_VERIFY(value != nullptr, "DatePickerCalendar needs a value to bind to");
        if (value == nullptr)
            return false;

        const ID id = GetID(label);
        const DateTime today = options.Today.value_or(GetCurrentDateTime());

        // An invalid or out-of-range value is corrected without reporting a change, like an out-of-range index.
        DateTime current = ClampDate(MakeValidDate(*value), options.MinDate, options.MaxDate);
        *value = current;
        const DateTime before = current;

        CalendarState& state = *GetState<CalendarState>(id);
        if (!state.HasMonth || state.ShownValue != current)
        {
            state.Year = current.Year;
            state.Month = current.Month;
            state.ShownValue = current;
            state.HasMonth = true;
        }

        PushDisabled(options.Disabled);
        const float gridTop = HeaderHeight + HeaderGap + WeekdayHeight;
        const Rect rect = AllocateItem(Vec2(CellWidth * Columns, gridTop + CellHeight * Weeks));
        DrawList& drawList = GetDrawList();
        const Color labelColor = GetStyleColor(StyleColor::Label);
        const Color secondary = GetStyleColor(StyleColor::SecondaryLabel);
        const Color accent = GetStyleColor(StyleColor::Accent);

        // Header: the month and year, then previous month, today, next month at the trailing edge.
        DateTime firstOfMonth;
        firstOfMonth.Year = state.Year;
        firstOfMonth.Month = state.Month;
        firstOfMonth.Day = 1;
        const DateTime lastOfPrevious = AddDays(firstOfMonth, -1);
        const DateTime firstOfNext = AddMonths(firstOfMonth, 1);
        struct NavigationButtonInfo
        {
            const char* Name;
            const char* Icon;
            float IconSize;
            bool IsEnabled;
        };
        const NavigationButtonInfo buttons[3] = {
            {"##previous", Icons::CaretLeft, 12.0f, IsDayInRange(lastOfPrevious, options) || !options.MinDate},
            {"##today", Icons::Circle, 9.0f, true},
            {"##next", Icons::CaretRight, 12.0f, IsDayInRange(firstOfNext, options) || !options.MaxDate},
        };
        for (int i = 0; i < 3; i++)
        {
            const Rect button(rect.GetRight() - NavigationButton * static_cast<float>(3 - i),
                              rect.Y + (HeaderHeight - NavigationButton) * 0.5f, NavigationButton, NavigationButton);
            const ID buttonID = HashID(buttons[i].Name, id);
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            behavior.Disabled = !buttons[i].IsEnabled;
            const Interaction interaction = ButtonBehavior(buttonID, button, behavior);
            if (interaction.Clicked)
            {
                const DateTime shown = i == 0 ? lastOfPrevious : (i == 2 ? firstOfNext : today);
                state.Year = shown.Year;
                state.Month = shown.Month;
            }
            const ControlFeedback feedback = AnimateFeedback(buttonID, interaction.Hovered, interaction.Pressed);
            drawList.AddSquircle(button, ApplyFeedback(labelColor.WithOpacity(0.0f), labelColor, feedback),
                                 NavigationButton * 0.5f, 0.0f);
            DrawIcon(drawList, button.GetCenter(), buttons[i].Icon, buttons[i].IconSize,
                     buttons[i].IsEnabled ? secondary : GetStyleColor(StyleColor::QuaternaryLabel),
                     i == 1 ? IconVariant::Fill : IconVariant::Bold);
        }

        char title[32] = {};
        const std::string_view monthName = GetMonthName(state.Month);
        std::memcpy(title, monthName.data(), monthName.size());
        title[monthName.size()] = ' ';
        const char* titleEnd = std::to_chars(title + monthName.size() + 1, title + sizeof(title), state.Year).ptr;
        DrawLabel(drawList, Rect(rect.X, rect.Y, rect.Width, HeaderHeight), rect.X + TitleInset,
                  std::string_view(title, static_cast<size_t>(titleEnd - title)), GetTextSpec(TextStyle::Headline),
                  labelColor);

        // Weekday letters, starting with the configured first day of the week.
        const TextSpec weekdaySpec = GetTextSpec(TextStyle::Subheadline);
        for (int column = 0; column < Columns; column++)
        {
            const Weekday weekday = static_cast<Weekday>((static_cast<int>(options.FirstWeekday) + column) % Columns);
            const std::string_view letter = GetWeekdayName(weekday).substr(0, 1);
            const Rect cell(rect.X + CellWidth * static_cast<float>(column), rect.Y + HeaderHeight + HeaderGap,
                            CellWidth, WeekdayHeight);
            DrawLabel(drawList, cell, cell.GetCenter().X - MeasureText(letter, weekdaySpec).X * 0.5f, letter,
                      weekdaySpec, secondary);
        }

        // The grid starts on the first weekday on or before the first of the month.
        const int offset =
            (static_cast<int>(GetWeekday(firstOfMonth)) - static_cast<int>(options.FirstWeekday) + Columns) % Columns;
        const DateTime gridStart = AddDays(firstOfMonth, -offset);
        const Rect grid(rect.X, rect.Y + gridTop, CellWidth * Columns, CellHeight * Weeks);
        const auto getCellRect = [&grid](int index)
        {
            return Rect(grid.X + CellWidth * static_cast<float>(index % Columns),
                        grid.Y + CellHeight * static_cast<float>(index / Columns), CellWidth, CellHeight);
        };
        const auto withTime = [&current](const DateTime& day)
        {
            DateTime result = day;
            result.Hour = current.Hour;
            result.Minute = current.Minute;
            return result;
        };

        // The grid is one stop for Tab; the arrow keys move the selected day.
        RegisterFocusable(id, grid);
        Interaction summary;
        summary.Hovered = IsRectHovered(rect);
        Interaction interactions[Cells];
        bool enabled[Cells];
        for (int i = 0; i < Cells; i++)
        {
            const DateTime day = AddDays(gridStart, i);
            enabled[i] = IsDayInRange(day, options);
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            behavior.Disabled = !enabled[i];
            interactions[i] = ButtonBehavior(HashID(i, id), getCellRect(i), behavior);
            summary.Pressed = summary.Pressed || interactions[i].Pressed;
            if (interactions[i].Clicked)
            {
                // A day of the neighbouring month selects it, and the calendar moves to that month.
                current = ClampDate(withTime(day), options.MinDate, options.MaxDate);
                SetFocus(id);
            }
        }
        if (IsFocused(id) && !IsDisabled())
        {
            DateTime moved = current;
            const bool shift = IsKeyDown(Key::LeftShift) || IsKeyDown(Key::RightShift);
            if (IsKeyPressed(Key::LeftArrow))
                moved = AddDays(moved, -1);
            if (IsKeyPressed(Key::RightArrow))
                moved = AddDays(moved, 1);
            if (IsKeyPressed(Key::UpArrow))
                moved = AddDays(moved, -Columns);
            if (IsKeyPressed(Key::DownArrow))
                moved = AddDays(moved, Columns);
            if (IsKeyPressed(Key::PageUp))
                moved = AddMonths(moved, shift ? -12 : -1);
            if (IsKeyPressed(Key::PageDown))
                moved = AddMonths(moved, shift ? 12 : 1);
            if (IsKeyPressed(Key::Home, false))
                moved.Day = 1;
            if (IsKeyPressed(Key::End, false))
                moved.Day = GetDaysInMonth(moved.Year, moved.Month);
            current = ClampDate(moved, options.MinDate, options.MaxDate);
        }

        const bool changed = current != before;
        if (changed)
        {
            *value = current;
            state.Year = current.Year;
            state.Month = current.Month;
            state.ShownValue = current;
        }

        // Days: those of other months and those out of range dimmed, today in the accent color, the selected day
        // on an accent circle.
        const TextSpec daySpec = GetTextSpec(TextStyle::Body);
        TextSpec todaySpec = daySpec;
        todaySpec.Weight = FontWeight::Semibold;
        Rect selectedCircle;
        bool isSelectedShown = false;
        for (int i = 0; i < Cells; i++)
        {
            const DateTime day = AddDays(gridStart, i);
            const Rect cell = getCellRect(i);
            const Rect circle(cell.GetCenter().X - DayDiameter * 0.5f, cell.GetCenter().Y - DayDiameter * 0.5f,
                              DayDiameter, DayDiameter);
            const bool isSelected = IsSameDay(day, current);
            const bool isToday = IsSameDay(day, today);
            const bool isInMonth = day.Month == firstOfMonth.Month;

            const ControlFeedback feedback =
                AnimateFeedback(HashID(i, id), interactions[i].Hovered, interactions[i].Pressed);
            Color text = isInMonth ? labelColor : GetStyleColor(StyleColor::TertiaryLabel);
            if (isToday)
                text = accent;
            if (!enabled[i])
                text = GetStyleColor(StyleColor::QuaternaryLabel);
            if (isSelected)
            {
                drawList.AddSquircle(circle, ApplyFeedback(accent, labelColor, feedback), DayDiameter * 0.5f, 0.0f);
                text = GetStyleColor(StyleColor::OnAccent);
                selectedCircle = circle;
                isSelectedShown = true;
            }
            else
            {
                drawList.AddSquircle(circle, ApplyFeedback(labelColor.WithOpacity(0.0f), labelColor, feedback),
                                     DayDiameter * 0.5f, 0.0f);
            }

            char buffer[8];
            const std::string_view number = FormatNumber(buffer, day.Day);
            const TextSpec& spec = isToday ? todaySpec : daySpec;
            DrawLabel(drawList, cell, cell.GetCenter().X - MeasureText(number, spec).X * 0.5f, number, spec, text);
        }

        if (isSelectedShown)
            DrawFocusRing(id, selectedCircle, DayDiameter * 0.5f);
        else
            DrawFocusRing(id, grid, GetStyleVar(StyleVar::CornerRadius));
        summary.Focused = IsFocused(id);
        summary.FocusVisible = IsFocusVisible(id);
        summary.Clicked = changed;
        SetLastItem(id, rect, summary);
        PopDisabled();
        return changed;
    }
} // namespace Carbon
