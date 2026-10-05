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
