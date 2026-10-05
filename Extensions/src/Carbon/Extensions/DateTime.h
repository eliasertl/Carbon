#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Carbon
{
    /// The days of the week. The values match C's and std::chrono's encoding: Sunday is 0.
    enum class Weekday : uint8_t
    {
        Sunday,
        Monday,
        Tuesday,
        Wednesday,
        Thursday,
        Friday,
        Saturday
    };

    /// A date and a time of day in the proleptic Gregorian calendar, as the application keeps it. It is local wall
    /// time: there is no time zone, and Carbon never converts it.
    struct DateTime
    {
        int Year = 2000;
        /// 1 (January) to 12.
        int Month = 1;
        /// 1 to the length of the month.
        int Day = 1;
        /// 0 to 23.
        int Hour = 0;
        /// 0 to 59.
        int Minute = 0;

        /// Chronological order.
        auto operator<=>(const DateTime&) const = default;
    };

    /// True when every field is in its range and the day exists in its month.
    bool IsValidDate(const DateTime& date);
    /// The nearest valid date: each field clamped to its range, the day to the length of its month.
    DateTime MakeValidDate(const DateTime& date);
    /// 28 to 31.
    int GetDaysInMonth(int year, int month);
    bool IsLeapYear(int year);
    Weekday GetWeekday(const DateTime& date);
    /// Moves by whole days, across months and years. The time of day is kept.
    DateTime AddDays(const DateTime& date, int days);
    /// Moves by whole months; a day that does not exist in the new month becomes its last day (January 31 plus
    /// one month is February 28 or 29). The time of day is kept.
    DateTime AddMonths(const DateTime& date, int months);
    /// Whether two values fall on the same day.
    bool IsSameDay(const DateTime& a, const DateTime& b);
    /// The value moved into [min, max] where those are set.
    DateTime ClampDate(const DateTime& date, const std::optional<DateTime>& min, const std::optional<DateTime>& max);
    /// The current date and time of the computer's clock, in its local time.
    DateTime GetCurrentDateTime();

    /// English names: "January" to "December" for 1 to 12.
    std::string_view GetMonthName(int month);
    /// "Sunday" to "Saturday".
    std::string_view GetWeekdayName(Weekday weekday);
} // namespace Carbon
