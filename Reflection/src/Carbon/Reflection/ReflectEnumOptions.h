#pragma once

#include <string_view>

namespace Carbon
{
    /// The range of values searched for enumerators when an enum is reflected automatically. Values outside it are
    /// found only if CB_REFLECT_ENUM names them with CB_VALUE.
    struct ReflectEnumOptions
    {
        /// Smallest value searched.
        int Min = -128;
        /// Largest value searched. At most MaxReflectedEnumRange values are searched.
        int Max = 127;
    };

    /// The most values the automatic search covers for one enum (Max - Min + 1).
    inline constexpr int MaxReflectedEnumRange = 1024;

    /// Metadata of one enum value, given with CB_VALUE. Every field is optional.
    struct ReflectValueOptions
    {
        /// The label; empty derives it from the enumerator ("VeryHigh" becomes "Very High").
        std::string_view DisplayName = {};
        /// Leaves the value out: it is not counted, listed or offered (for example a trailing `Count`).
        bool Hidden = false;
    };
} // namespace Carbon
