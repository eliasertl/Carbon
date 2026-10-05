#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Carbon/Reflection/Detail/Description.h"
#include "Carbon/Reflection/Detail/DisplayName.h"
#include "Carbon/Reflection/Detail/Signature.h"
#include "Carbon/Reflection/ReflectEnumOptions.h"

namespace Carbon::Internal
{
    /// The macro description of an enum, or an empty one with the default range.
    template <typename E>
    constexpr auto GetEnumDescription() noexcept
    {
        if constexpr (HasReflectDescription<E>)
            return GetReflectDescription<E>();
        else
            return EnumDescription<E, 0>{};
    }

    /// One value of a reflected enum.
    template <typename E>
    struct EnumEntry
    {
        int64_t Number = 0;
        E Value = {};
        std::string_view Name = {};
        std::string_view DisplayName = {};
        bool Hidden = false;
    };

    /// The range searched for enumerators: the description's, clamped to what the underlying type can hold.
    template <typename E>
    struct EnumRange
    {
        int64_t First = 0;
        int64_t Last = -1;
        size_t Count = 0;
    };

    template <typename E>
    constexpr EnumRange<E> GetEnumRange() noexcept
    {
        using Underlying = std::underlying_type_t<E>;
        constexpr ReflectEnumOptions options = GetEnumDescription<E>().Options;
        EnumRange<E> range;
        range.First = std::max<int64_t>(options.Min, static_cast<int64_t>(std::numeric_limits<Underlying>::min()));
        if constexpr (std::is_unsigned_v<Underlying> && sizeof(Underlying) >= sizeof(int64_t))
            range.Last = options.Max;
        else
            range.Last = std::min<int64_t>(options.Max, static_cast<int64_t>(std::numeric_limits<Underlying>::max()));
        range.Count = range.Last >= range.First ? static_cast<size_t>(range.Last - range.First + 1) : 0;
        return range;
    }

    /// The enumerator names of every value in the range; empty for values without one.
    template <typename E, size_t... Index>
    constexpr std::array<std::string_view, sizeof...(Index)> ScanEnumNames(std::index_sequence<Index...>) noexcept
    {
        using Underlying = std::underlying_type_t<E>;
        constexpr int64_t first = GetEnumRange<E>().First;
        return {GetNameOf<static_cast<E>(static_cast<Underlying>(first + static_cast<int64_t>(Index)))>()...};
    }

    template <typename E, size_t Capacity>
    struct EnumEntries
    {
        std::array<EnumEntry<E>, Capacity> Entries = {};
        size_t Count = 0;
    };

    /// The enumerators found in the range, merged with the values the macro names, sorted by value, without the
    /// hidden ones.
    template <typename E>
    constexpr auto CollectEnumEntries() noexcept
    {
        using Underlying = std::underlying_type_t<E>;
        constexpr auto description = GetEnumDescription<E>();
        constexpr EnumRange<E> range = GetEnumRange<E>();
        static_assert(description.Options.Min <= description.Options.Max,
                      "CB_REFLECT_ENUM: the range's Min must not be larger than its Max");
        static_assert(range.Count <= static_cast<size_t>(MaxReflectedEnumRange),
                      "CB_REFLECT_ENUM: the range is too large; name values outside a smaller range with CB_VALUE");
        constexpr std::array<std::string_view, range.Count> scanned =
            ScanEnumNames<E>(std::make_index_sequence<range.Count>());
        constexpr size_t capacity = range.Count + description.Values.size();

        EnumEntries<E, capacity> all;
        for (size_t i = 0; i < range.Count; i++)
        {
            if (scanned[i].empty())
                continue;
            const int64_t number = range.First + static_cast<int64_t>(i);
            all.Entries[all.Count++] =
                EnumEntry<E>{number, static_cast<E>(static_cast<Underlying>(number)), scanned[i], {}, false};
        }
        for (const ValueDescription<E>& value : description.Values)
        {
            const int64_t number = static_cast<int64_t>(static_cast<Underlying>(value.Value));
            size_t position = 0;
            while (position < all.Count && all.Entries[position].Number < number)
                position++;
            if (position == all.Count || all.Entries[position].Number != number)
            {
                for (size_t i = all.Count; i > position; i--)
                    all.Entries[i] = all.Entries[i - 1];
                all.Entries[position] = EnumEntry<E>{number, value.Value, value.Name, {}, false};
                all.Count++;
            }
            all.Entries[position].DisplayName = value.Options.DisplayName;
            all.Entries[position].Hidden = value.Options.Hidden;
        }

        EnumEntries<E, capacity> visible;
        for (size_t i = 0; i < all.Count; i++)
        {
            if (!all.Entries[i].Hidden)
                visible.Entries[visible.Count++] = all.Entries[i];
        }
        return visible;
    }

    /// One member of every entry, as an array.
    template <size_t Count, typename Entries, typename Project>
    constexpr auto ProjectEnumEntries(const Entries& entries, Project project) noexcept
    {
        std::array<decltype(project(entries.Entries[0])), Count> result = {};
        for (size_t i = 0; i < Count; i++)
            result[i] = project(entries.Entries[i]);
        return result;
    }

    /// Everything reflection knows about an enum, computed while compiling.
    template <typename E>
    struct EnumModel
    {
        static_assert(std::is_enum_v<E>, "Only enums can be reflected as enums");
        using Underlying = std::underlying_type_t<E>;

        static constexpr auto All = CollectEnumEntries<E>();
        static constexpr size_t Count = All.Count;

        static constexpr std::array<E, Count> Values =
            ProjectEnumEntries<Count>(All, [](const EnumEntry<E>& entry) { return entry.Value; });
        static constexpr std::array<int64_t, Count> Numbers =
            ProjectEnumEntries<Count>(All, [](const EnumEntry<E>& entry) { return entry.Number; });
        static constexpr std::array<std::string_view, Count> Names =
            ProjectEnumEntries<Count>(All, [](const EnumEntry<E>& entry) { return entry.Name; });
        static constexpr std::array<std::string_view, Count> DisplayNames =
            ProjectEnumEntries<Count>(All, [](const EnumEntry<E>& entry) { return entry.DisplayName; });

        /// The index of a value, or -1 when it is not reflected. Binary search; the values are sorted.
        static constexpr int IndexOf(E value) noexcept
        {
            const int64_t number = static_cast<int64_t>(static_cast<Underlying>(value));
            const int64_t* end = Numbers.data() + Count;
            const int64_t* found = std::lower_bound(Numbers.data(), end, number);
            if (found == end || *found != number)
                return -1;
            return static_cast<int>(found - Numbers.data());
        }

        /// The labels, formatted on first use and kept in static storage.
        static const std::array<std::string_view, Count>& GetLabels() noexcept
        {
            static const LabelTable<Count, GetLabelCapacity(Names, DisplayNames)> table(Names, DisplayNames);
            return table.GetLabels();
        }
    };
} // namespace Carbon::Internal
