#include <Carbon/Extensions/Extensions.h>

#include "Support/WidgetTest.h"

namespace Carbon
{
    namespace
    {
        DateTime Date(int year, int month, int day, int hour = 0, int minute = 0)
        {
            return DateTime{.Year = year, .Month = month, .Day = day, .Hour = hour, .Minute = minute};
        }
    } // namespace

    // ---- DateTime -----------------------------------------------------------------------------------------------

    TEST(DateTimeTests, KnowsLeapYearsAndMonthLengths)
    {
        EXPECT_TRUE(IsLeapYear(2024));
        EXPECT_TRUE(IsLeapYear(2000));
        EXPECT_FALSE(IsLeapYear(1900));
        EXPECT_FALSE(IsLeapYear(2026));
        EXPECT_EQ(GetDaysInMonth(2024, 2), 29);
        EXPECT_EQ(GetDaysInMonth(1900, 2), 28);
        EXPECT_EQ(GetDaysInMonth(2026, 4), 30);
        EXPECT_EQ(GetDaysInMonth(2026, 12), 31);
    }

    TEST(DateTimeTests, KnowsWeekdays)
    {
        EXPECT_EQ(GetWeekday(Date(2026, 10, 5)), Weekday::Monday);
        EXPECT_EQ(GetWeekday(Date(2000, 1, 1)), Weekday::Saturday);
        EXPECT_EQ(GetWeekday(Date(2024, 2, 29)), Weekday::Thursday);
        EXPECT_EQ(GetWeekdayName(Weekday::Wednesday), "Wednesday");
        EXPECT_EQ(GetMonthName(10), "October");
    }

    TEST(DateTimeTests, MovesByDaysAndMonthsKeepingTheTime)
    {
        EXPECT_EQ(AddDays(Date(2026, 12, 31, 14, 30), 1), Date(2027, 1, 1, 14, 30));
        EXPECT_EQ(AddDays(Date(2024, 3, 1), -1), Date(2024, 2, 29));
        EXPECT_EQ(AddMonths(Date(2024, 1, 31, 9, 15), 1), Date(2024, 2, 29, 9, 15));
        EXPECT_EQ(AddMonths(Date(2026, 1, 15), -1), Date(2025, 12, 15));
        EXPECT_EQ(AddMonths(Date(2026, 3, 31), -13), Date(2025, 2, 28));
        EXPECT_EQ(AddMonths(Date(2026, 10, 5), 24), Date(2028, 10, 5));
    }

    TEST(DateTimeTests, ValidatesAndClamps)
    {
        EXPECT_TRUE(IsValidDate(Date(2024, 2, 29, 23, 59)));
        EXPECT_FALSE(IsValidDate(Date(2026, 2, 29)));
        EXPECT_FALSE(IsValidDate(Date(2026, 1, 1, 24, 0)));
        EXPECT_EQ(MakeValidDate(Date(2026, 13, 40, 25, 61)), Date(2026, 12, 31, 23, 59));
        EXPECT_EQ(MakeValidDate(Date(2026, 2, 30)), Date(2026, 2, 28));
        EXPECT_EQ(ClampDate(Date(2026, 1, 1), Date(2026, 3, 1), {}), Date(2026, 3, 1));
        EXPECT_EQ(ClampDate(Date(2026, 9, 1), {}, Date(2026, 6, 30)), Date(2026, 6, 30));
        EXPECT_EQ(ClampDate(Date(2026, 5, 1), Date(2026, 3, 1), Date(2026, 6, 30)), Date(2026, 5, 1));
        EXPECT_TRUE(Date(2026, 10, 5, 8, 0) < Date(2026, 10, 5, 9, 0));
        EXPECT_TRUE(IsSameDay(Date(2026, 10, 5, 8, 0), Date(2026, 10, 5, 23, 0)));
    }

    // ---- DatePickerCalendar -------------------------------------------------------------------------------------

    class DatePickerCalendarTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                if (DatePickerCalendar("calendar", &m_Value, m_Options))
                    m_Changes++;
                m_Rect = GetItemRect();
                m_Id = GetItemID();
            };
        }

        // The centre of a day of the grid: 7 columns of 32 points, 6 rows of 28 below the header.
        Vec2 GetCell(int column, int row) const
        {
            return Vec2(m_Rect.X + 32.0f * float(column) + 16.0f, m_Rect.Y + 52.0f + 28.0f * float(row) + 14.0f);
        }

        Vec2 GetCell(int index) const { return GetCell(index % 7, index / 7); }

        // The previous month, today and next month buttons, at the trailing end of the header.
        Vec2 GetButton(int index) const
        {
            return Vec2(m_Rect.GetRight() - 22.0f * float(3 - index) + 11.0f, m_Rect.Y + 14.0f);
        }

        DateTime m_Value = Date(2026, 10, 5, 14, 30);
        DatePickerCalendarOptions m_Options = {.Today = Date(2026, 10, 5)};
        int m_Changes = 0;
        Rect m_Rect;
        ID m_Id;
    };

    TEST_F(DatePickerCalendarTests, ClickingADaySelectsItAndKeepsTheTime)
    {
        Settle(Interface());
        // October 2026 starts on a Thursday: with Monday first, the 1st is in the fourth column.
        Click(GetCell(3, 0), Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 1, 14, 30));
        Click(GetCell(4, 2), Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 16, 14, 30));
        EXPECT_EQ(m_Changes, 2);
        EXPECT_EQ(GetFocusedID(), m_Id);
    }

    TEST_F(DatePickerCalendarTests, MonthsStartingOnEveryWeekdayAreLaidOutForBothFirstWeekdays)
    {
        for (const Weekday first : {Weekday::Monday, Weekday::Sunday})
        {
            for (int weekday = 0; weekday < 7; weekday++)
            {
                // The first month from 2026 on that starts on this weekday.
                DateTime month = Date(2026, 1, 1);
                while (static_cast<int>(GetWeekday(month)) != weekday)
                    month = AddMonths(month, 1);
                SCOPED_TRACE(std::string(GetMonthName(month.Month)) + " " + std::to_string(month.Year) +
                             (first == Weekday::Monday ? ", Monday first" : ", Sunday first"));

                m_Options.FirstWeekday = first;
                m_Value = Date(month.Year, month.Month, 15);
                Settle(Interface());
                const int column = (weekday - static_cast<int>(first) + 7) % 7;
                Click(GetCell(column, 0), Interface());
                EXPECT_EQ(m_Value, Date(month.Year, month.Month, 1));
                MoveMouse(Vec2(700.0f, 500.0f), Interface());
            }
        }
    }

    TEST_F(DatePickerCalendarTests, LeapYearsAndDaysOfNeighbouringMonths)
    {
        // February 2024 starts on a Thursday and has 29 days.
        m_Value = Date(2024, 2, 15);
        Settle(Interface());
        Click(GetCell(3 + 28), Interface());
        EXPECT_EQ(m_Value, Date(2024, 2, 29));
        // The next cell is March 1: picking it moves the calendar to March, where the 1st is a Friday.
        Click(GetCell(3 + 29), Interface());
        EXPECT_EQ(m_Value, Date(2024, 3, 1));
        Settle(Interface());
        Click(GetCell(4, 0), Interface());
        EXPECT_EQ(m_Value, Date(2024, 3, 1)) << "the 1st is now in the first row";
        Click(GetCell(5, 0), Interface());
        EXPECT_EQ(m_Value, Date(2024, 3, 2));

        // February 2026 starts on a Sunday: with Sunday first its 28 days fill exactly four rows.
        m_Options.FirstWeekday = Weekday::Sunday;
        m_Value = Date(2026, 2, 10);
        Settle(Interface());
        Click(GetCell(6, 3), Interface());
        EXPECT_EQ(m_Value, Date(2026, 2, 28));
        Click(GetCell(0, 4), Interface());
        EXPECT_EQ(m_Value, Date(2026, 3, 1));
    }

    TEST_F(DatePickerCalendarTests, ArrowAndPageKeysMoveTheSelection)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        ASSERT_EQ(GetFocusedID(), m_Id);
        TapKey(Key::RightArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 6, 14, 30));
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 13, 14, 30));
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 14, 30));
        TapKey(Key::PageDown, Interface());
        EXPECT_EQ(m_Value, Date(2026, 11, 5, 14, 30));
        TapKey(Key::LeftShift, Key::PageDown, Interface());
        EXPECT_EQ(m_Value, Date(2027, 11, 5, 14, 30));
        TapKey(Key::PageUp, Interface());
        EXPECT_EQ(m_Value, Date(2027, 10, 5, 14, 30));
        TapKey(Key::End, Interface());
        EXPECT_EQ(m_Value, Date(2027, 10, 31, 14, 30));
        TapKey(Key::Home, Interface());
        EXPECT_EQ(m_Value, Date(2027, 10, 1, 14, 30));
        // Across a month boundary, and the grid follows.
        TapKey(Key::LeftArrow, Interface());
        EXPECT_EQ(m_Value, Date(2027, 9, 30, 14, 30));
    }

    TEST_F(DatePickerCalendarTests, MinAndMaxDisableDaysAndLimitTheKeys)
    {
        m_Options.MinDate = Date(2026, 10, 3);
        m_Options.MaxDate = Date(2026, 10, 24, 23, 59);
        m_Value = Date(2026, 10, 4);
        Settle(Interface());
        Click(GetCell(3, 0), Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 4)) << "October 1 is before the minimum";
        Click(GetCell(6, 3), Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 4)) << "October 25 is after the maximum";

        TapKey(Key::Tab, Interface());
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 3)) << "a week back is clamped to the minimum";
        TapKey(Key::PageDown, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 24, 23, 59)) << "a month on is clamped to the maximum";

        // A value outside the range is corrected without counting as a change.
        const int changes = m_Changes;
        m_Value = Date(2027, 1, 1);
        Settle(Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 24, 23, 59));
        EXPECT_EQ(m_Changes, changes);
    }

    TEST_F(DatePickerCalendarTests, ButtonsShowOtherMonthsWithoutChangingTheValue)
    {
        Settle(Interface());
        Click(GetButton(2), Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 14, 30));
        Settle(Interface());
        // November 2026 starts on a Sunday: the last column of the first row.
        Click(GetCell(6, 0), Interface());
        EXPECT_EQ(m_Value, Date(2026, 11, 1, 14, 30));

        Click(GetButton(0), Interface());
        Click(GetButton(0), Interface());
        Settle(Interface());
        // September 2026 starts on a Tuesday.
        Click(GetCell(1, 0), Interface());
        EXPECT_EQ(m_Value, Date(2026, 9, 1, 14, 30));

        Click(GetButton(2), Interface());
        Click(GetButton(2), Interface());
        Click(GetButton(1), Interface());
        Settle(Interface());
        // Back at today's month, October.
        Click(GetCell(3, 0), Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 1, 14, 30));
    }

    // ---- DatePicker ---------------------------------------------------------------------------------------------

    TEST(DatePickerFormatTests, FormatsTheThreeStyles)
    {
        char buffer[32];
        const DateTime value = Date(2026, 10, 5, 14, 30);
        const auto format = [&](const DateTime& date, DatePickerElements elements, const DateFormat& style)
        { return std::string(FormatDateTime(date, elements, style, buffer)); };

        EXPECT_EQ(format(value, DatePickerElements::Date, DateFormat::ISO()), "2026-10-05");
        EXPECT_EQ(format(value, DatePickerElements::Time, DateFormat::ISO()), "14:30");
        EXPECT_EQ(format(value, DatePickerElements::DateAndTime, DateFormat::ISO()), "2026-10-05 14:30");
        EXPECT_EQ(format(value, DatePickerElements::Date, DateFormat::German()), "05.10.2026");
        EXPECT_EQ(format(value, DatePickerElements::DateAndTime, DateFormat::German()), "05.10.2026, 14:30");
        EXPECT_EQ(format(value, DatePickerElements::Date, DateFormat::US()), "10/5/2026");
        EXPECT_EQ(format(value, DatePickerElements::DateAndTime, DateFormat::US()), "10/5/2026, 2:30 PM");

        // The ends of the 12-hour clock, and padding.
        EXPECT_EQ(format(Date(2026, 1, 9, 0, 30), DatePickerElements::Time, DateFormat::US()), "12:30 AM");
        EXPECT_EQ(format(Date(2026, 1, 9, 12, 0), DatePickerElements::Time, DateFormat::US()), "12:00 PM");
        EXPECT_EQ(format(Date(2026, 1, 9, 9, 5), DatePickerElements::Time, DateFormat::ISO()), "09:05");
        EXPECT_EQ(format(Date(2026, 1, 9, 9, 5), DatePickerElements::Date, DateFormat::German()), "09.01.2026");

        // A custom format: day, month and year with slashes, 24-hour time.
        DateFormat british = DateFormat::German();
        british.DateSeparator = '/';
        EXPECT_EQ(format(value, DatePickerElements::DateAndTime, british), "05/10/2026, 14:30");
    }

    class DatePickerTests : public WidgetTest
    {
    protected:
        Builder Interface()
        {
            return [this]
            {
                // Away from the edges, so that the popover opens below the field without being moved.
                BeginVStack({.Padding = EdgeInsets(300.0f, 100.0f)});
                if (DatePicker("picker", &m_Value, m_Options))
                    m_Changes++;
                m_Rect = GetItemRect();
                m_Id = GetItemID();
                EndVStack();
            };
        }

        // The centre of the text of the element that starts after `before` (the field's text up to it).
        Vec2 GetElement(std::string_view before, std::string_view element) const
        {
            const TextSpec spec = GetTextSpec(TextStyle::Body);
            const float x = m_Rect.X + 7.0f + MeasureText(before, spec).X + MeasureText(element, spec).X * 0.5f;
            return Vec2(x, m_Rect.GetCenter().Y);
        }

        // A day of the calendar in the popover: 12 points of padding below the 2-point gap and the 7-point arrow.
        Vec2 GetCalendarCell(int column, int row) const
        {
            const Vec2 origin(m_Rect.X + 12.0f, m_Rect.GetBottom() + 2.0f + 7.0f + 12.0f);
            return Vec2(origin.X + 32.0f * float(column) + 16.0f, origin.Y + 52.0f + 28.0f * float(row) + 14.0f);
        }

        ID GetPopover() { return HashID("##popover", m_Id); }

        DateTime m_Value = Date(2026, 10, 5, 14, 30);
        DatePickerOptions m_Options = {.Today = Date(2026, 10, 5)};
        int m_Changes = 0;
        Rect m_Rect;
        ID m_Id;
    };

    TEST_F(DatePickerTests, KeysStepAndTypeTheElements)
    {
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        ASSERT_EQ(GetFocusedID(), m_Id);
        // Tab starts at the first element, the year.
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2027, 10, 5, 14, 30));
        TapKey(Key::RightArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Value, Date(2027, 9, 5, 14, 30));

        // Two digits complete the month and move on to the day; a 4 cannot start a day, so it is the day.
        Type("12", Interface());
        EXPECT_EQ(m_Value, Date(2027, 12, 5, 14, 30));
        Type("4", Interface());
        EXPECT_EQ(m_Value, Date(2027, 12, 4, 14, 30));
        Type("31", Interface());
        EXPECT_EQ(m_Value, Date(2027, 12, 31, 14, 30));
        // Stepping wraps within the month, as on macOS.
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2027, 12, 1, 14, 30));

        // A separator after a complete element only confirms the move; a day that the month does not have
        // becomes its last day.
        TapKey(Key::LeftArrow, Interface());
        Type("2-", Interface());
        Type("30", Interface());
        EXPECT_EQ(m_Value, Date(2027, 2, 28, 14, 30));
        // Two-digit years are this century.
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::LeftArrow, Interface());
        Type("30-", Interface());
        EXPECT_EQ(m_Value, Date(2030, 2, 28, 14, 30));
        EXPECT_TRUE(m_AssertMessages.empty());
    }

    TEST_F(DatePickerTests, ClickingAnElementSelectsIt)
    {
        Settle(Interface());
        Click(GetElement("2026-10-", "05"), Interface());
        EXPECT_EQ(GetFocusedID(), m_Id);
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 6, 14, 30));
        Click(GetElement("2026-", "10"), Interface());
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 11, 6, 14, 30));
    }

    TEST_F(DatePickerTests, TwelveHourTimeTakesAmAndPm)
    {
        m_Options.Elements = DatePickerElements::DateAndTime;
        m_Options.Format = DateFormat::US();
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        // Month, day, year, hour.
        for (int i = 0; i < 3; i++)
            TapKey(Key::RightArrow, Interface());
        Type("9", Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 21, 30)) << "the hour keeps PM";
        Type("a", Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 9, 30));
        Type("12", Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 9, 12)) << "after the hour, the minute";
        // The minute moved on to AM/PM; two steps back is the hour.
        TapKey(Key::LeftArrow, Interface());
        TapKey(Key::LeftArrow, Interface());
        Type("12", Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 0, 12)) << "12 AM is midnight";
        Type("p", Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 12, 12));
    }

    TEST_F(DatePickerTests, MinutesFollowTheInterval)
    {
        m_Options.Elements = DatePickerElements::Time;
        m_Options.MinuteInterval = 15;
        m_Value = Date(2026, 10, 5, 7, 15);
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        TapKey(Key::RightArrow, Interface());
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 7, 30));
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        TapKey(Key::DownArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 7, 45)) << "wraps from 0 to 45";
        Type("44", Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 5, 7, 30)) << "a typed minute is rounded down to the interval";
    }

    TEST_F(DatePickerTests, TheStepperChangesTheSelectedElement)
    {
        Settle(Interface());
        Click(GetElement("2026-", "10"), Interface());
        // The stepper is 4 points to the right of the field: its upper half adds one.
        Click(Vec2(m_Rect.GetRight() + 4.0f + 7.0f, m_Rect.Y + 6.0f), Interface());
        EXPECT_EQ(m_Value, Date(2026, 11, 5, 14, 30));
        Click(Vec2(m_Rect.GetRight() + 4.0f + 7.0f, m_Rect.GetBottom() - 6.0f), Interface());
        Click(Vec2(m_Rect.GetRight() + 4.0f + 7.0f, m_Rect.GetBottom() - 6.0f), Interface());
        EXPECT_EQ(m_Value, Date(2026, 9, 5, 14, 30));
    }

    TEST_F(DatePickerTests, TheCalendarPopoverPicksADay)
    {
        Settle(Interface());
        Click(GetElement("", "2026"), Interface());
        EXPECT_TRUE(IsOverlayOpen(GetPopover()));
        Settle(Interface());
        // October 2026 starts on a Thursday: the 20th is the Tuesday of the fourth week.
        Click(GetCalendarCell(1, 3), Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 20, 14, 30)) << "the day changes, the time stays";
        EXPECT_FALSE(IsOverlayOpen(GetPopover()));
        EXPECT_EQ(GetFocusedID(), m_Id) << "the field keeps the keyboard";

        // Space opens and Escape closes it from the keyboard; typing goes on meanwhile.
        TapKey(Key::Space, Interface());
        EXPECT_TRUE(IsOverlayOpen(GetPopover()));
        TapKey(Key::Escape, Interface());
        EXPECT_FALSE(IsOverlayOpen(GetPopover()));

        // A click elsewhere closes it too.
        TapKey(Key::Space, Interface());
        Settle(Interface());
        Click(Vec2(780.0f, 590.0f), Interface());
        EXPECT_FALSE(IsOverlayOpen(GetPopover()));
    }

    TEST_F(DatePickerTests, ValuesStayWithinMinAndMax)
    {
        m_Options.Elements = DatePickerElements::DateAndTime;
        m_Options.MinDate = Date(2026, 10, 1);
        m_Options.MaxDate = Date(2026, 10, 31, 23, 59);
        Settle(Interface());
        TapKey(Key::Tab, Interface());
        TapKey(Key::UpArrow, Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 31, 23, 59)) << "a year later is clamped to the maximum";
        Type("2025-", Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 1)) << "a typed year before the range is clamped to the minimum";

        // An out-of-range value from the application is corrected without counting as a change.
        const int changes = m_Changes;
        m_Value = Date(2030, 1, 1);
        Settle(Interface());
        EXPECT_EQ(m_Value, Date(2026, 10, 31, 23, 59));
        EXPECT_EQ(m_Changes, changes);
    }

    TEST_F(DatePickerTests, DisabledIgnoresInput)
    {
        m_Options.Disabled = true;
        Settle(Interface());
        Click(GetElement("", "2026"), Interface());
        EXPECT_FALSE(IsOverlayOpen(GetPopover()));
        TapKey(Key::Tab, Interface());
        EXPECT_FALSE(GetFocusedID().IsValid());
        EXPECT_EQ(m_Changes, 0);
    }

    TEST_F(DatePickerCalendarTests, InvalidValuesAreCorrectedAndDisabledIgnoresInput)
    {
        m_Value = Date(2026, 2, 31, 30, 99);
        Settle(Interface());
        EXPECT_EQ(m_Value, Date(2026, 2, 28, 23, 59));
        EXPECT_EQ(m_Changes, 0);

        m_Options.Disabled = true;
        Settle(Interface());
        Click(GetCell(0, 1), Interface());
        EXPECT_EQ(m_Value, Date(2026, 2, 28, 23, 59));
        TapKey(Key::Tab, Interface());
        EXPECT_FALSE(GetFocusedID().IsValid());
        EXPECT_TRUE(m_AssertMessages.empty());
    }
} // namespace Carbon
