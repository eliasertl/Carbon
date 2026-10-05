#include "Carbon/Extensions/DateTime.h"

#include <algorithm>
#include <chrono>
#include <ctime>

namespace Carbon
{
    namespace
    {
        // Calendar arithmetic goes through std::chrono, which covers years -32767 to 32767.
        constexpr int MinYear = -32767;
        constexpr int MaxYear = 32767;

        std::chrono::year_month_day ToYearMonthDay(const DateTime& date)
        {
            return std::chrono::year_month_day(std::chrono::year(date.Year),
                                               std::chrono::month(static_cast<unsigned>(date.Month)),
                                               std::chrono::day(static_cast<unsigned>(date.Day)));
        }

        DateTime FromYearMonthDay(const std::chrono::year_month_day& ymd, const DateTime& time)
        {
            DateTime result = time;
            result.Year = static_cast<int>(ymd.year());
            result.Month = static_cast<int>(static_cast<unsigned>(ymd.month()));
            result.Day = static_cast<int>(static_cast<unsigned>(ymd.day()));
            return result;
        }
    } // namespace

    bool IsLeapYear(int year)
    {
        return std::chrono::year(std::clamp(year, MinYear, MaxYear)).is_leap();
    }

    int GetDaysInMonth(int year, int month)
    {
        const std::chrono::year_month_day_last last(
            std::chrono::year(std::clamp(year, MinYear, MaxYear)),
            std::chrono::month_day_last(std::chrono::month(static_cast<unsigned>(std::clamp(month, 1, 12)))));
        return static_cast<int>(static_cast<unsigned>(last.day()));
    }

    bool IsValidDate(const DateTime& date)
    {
        return date.Year >= MinYear && date.Year <= MaxYear && date.Month >= 1 && date.Month <= 12 && date.Day >= 1 &&
               date.Day <= GetDaysInMonth(date.Year, date.Month) && date.Hour >= 0 && date.Hour <= 23 &&
               date.Minute >= 0 && date.Minute <= 59;
    }

    DateTime MakeValidDate(const DateTime& date)
    {
        DateTime result;
        result.Year = std::clamp(date.Year, MinYear, MaxYear);
        result.Month = std::clamp(date.Month, 1, 12);
        result.Day = std::clamp(date.Day, 1, GetDaysInMonth(result.Year, result.Month));
        result.Hour = std::clamp(date.Hour, 0, 23);
        result.Minute = std::clamp(date.Minute, 0, 59);
        return result;
    }

    Weekday GetWeekday(const DateTime& date)
    {
        const std::chrono::weekday weekday{std::chrono::sys_days(ToYearMonthDay(MakeValidDate(date)))};
        return static_cast<Weekday>(weekday.c_encoding());
    }

    DateTime AddDays(const DateTime& date, int days)
    {
        const DateTime valid = MakeValidDate(date);
        const std::chrono::sys_days moved = std::chrono::sys_days(ToYearMonthDay(valid)) + std::chrono::days(days);
        return FromYearMonthDay(std::chrono::year_month_day(moved), valid);
    }

    DateTime AddMonths(const DateTime& date, int months)
    {
        const DateTime valid = MakeValidDate(date);
        // Count months from year 0 so that negative steps carry into the year correctly.
        const long long index = static_cast<long long>(valid.Year) * 12 + (valid.Month - 1) + months;
        DateTime result = valid;
        result.Year =
            static_cast<int>(std::clamp<long long>(index >= 0 ? index / 12 : -((-index + 11) / 12), MinYear, MaxYear));
        result.Month = static_cast<int>(index - static_cast<long long>(result.Year) * 12) + 1;
        result.Month = std::clamp(result.Month, 1, 12);
        result.Day = std::min(valid.Day, GetDaysInMonth(result.Year, result.Month));
        return result;
    }

    bool IsSameDay(const DateTime& a, const DateTime& b)
    {
        return a.Year == b.Year && a.Month == b.Month && a.Day == b.Day;
    }

    DateTime ClampDate(const DateTime& date, const std::optional<DateTime>& min, const std::optional<DateTime>& max)
    {
        DateTime result = date;
        if (min.has_value() && result < *min)
            result = *min;
        if (max.has_value() && result > *max)
            result = *max;
        return result;
    }

    DateTime GetCurrentDateTime()
    {
        // The operating system knows the local time zone; Carbon only asks it for the wall clock.
        const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local = {};
#if defined(_WIN32)
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        DateTime result;
        result.Year = local.tm_year + 1900;
        result.Month = local.tm_mon + 1;
        result.Day = local.tm_mday;
        result.Hour = local.tm_hour;
        result.Minute = local.tm_min;
        return result;
    }

    std::string_view GetMonthName(int month)
    {
        static constexpr std::string_view Names[] = {"January",   "February", "March",    "April",
                                                     "May",       "June",     "July",     "August",
                                                     "September", "October",  "November", "December"};
        return Names[std::clamp(month, 1, 12) - 1];
    }

    std::string_view GetWeekdayName(Weekday weekday)
    {
        static constexpr std::string_view Names[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                                     "Thursday", "Friday", "Saturday"};
        return Names[std::min(static_cast<size_t>(weekday), size_t(6))];
    }
} // namespace Carbon
